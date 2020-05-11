/****************************************************************************
 Brenden Nelson (011635716) and Puthypor Sengkeo (011646389)ext2 file system
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ext2fs/ext2_fs.h>
#include <string.h>
#include <libgen.h>
#include <sys/stat.h>
#include <time.h>
#include <stdbool.h>
#include <sys/types.h>

//helper files
#include "type.h"

bool isreadlink = false;
bool isunlink = false;
bool ismount = false;

// global variables
MINODE minode[NMINODE];
MINODE *root;
MountTable mtable[NMTABLE];

PROC   proc[NPROC], *running;
OFT oft[NFD]; //global openfiletable struct

char cmd[32], pathname[128];
char gpath[128]; // global for tokenized components
char *name[32];  // assume at most 32 components in pathname
int   n;         // number of component strings
struct stat myst;//keeps info of file for most recent stat call

int fd, dev;
int nblocks, ninodes, bmap, imap, inode_start; // disk parameters

#include "util.c"

//lvl 1 files
#include "cd_ls_pwd.c"
#include "mkdir_creat.c"
#include "rmdir.c"
#include "link_unlink.c"
#include "symlink.c"
#include "misc1.c"

//lvl 2 files
#include "open_close_lseek.c"
#include "read_cat.c"
#include "write_cp.c"

//lvl 3 files
#include "mount_umount.c"

// char *disk = "disk1";
char *disk = "disk2";
// char *disk = "disk3.1";
int init() //initializes MINODE and PROC
{
  int i, j;
  MINODE *mip;
  PROC   *p;

  printf("init()\n");
  //initializing all mtable entries
  for (i = 0; i < NMTABLE; i++)
  {
    mtable[i].dev = 0;
  }
  //setting first entry in mtable to opening filesys
  strcpy(mtable[0].name, disk);
  strcpy(mtable[0].mntname, "/");
  mtable[0].dev = dev;

  //initializing all minodes
  for (i = 0; i < NMINODE; i++)
  {
    mip = &minode[i];
    mip->dev = mip->ino = 0;
    mip->refCount = 0;
    mip->mounted = 0;
    mip->mptr = 0;
  }
  for (i=0; i<NPROC; i++){
    p = &proc[i];
    p->pid = i;//setting process id's
    p->uid = i;//p0 is superuser
    p->gid = 0;
    p->cwd = 0;
    p->status = FREE;//ready to run
    for (j=0; j<NFD; j++) //all file descriptors as null
      p->fd[j] = 0;
  }
  proc[NPROC - 1].next = &proc[0];//circular list
  //p0 created as running process
  running = &proc[0];
}

// load root INODE and set root pointer to it
int mount_root()
{  
  printf("mount_root()\n");
  root = iget(dev, 2);
  //root->INODE.i_mode = 0777;
}

int main(int argc, char *argv[])
{
  int ino;
  char buf[BLKSIZE];
  char line[128];

  printf("checking EXT2 FS ....");
  if ((fd = open(disk, O_RDWR)) < 0){
    printf("open %s failed\n", disk);
    exit(1);
  }
  dev = fd;    // fd is the global dev 

  /********** read super block  ****************/
  get_block(dev, 1, buf);
  sp = (SUPER *)buf;

  /* verify it's an ext2 file system ***********/
  if (sp->s_magic != 0xEF53){
      printf("magic = %x is not an ext2 filesystem\n", sp->s_magic);
      exit(1);
  }     
  printf("EXT2 FS OK\n");
  ninodes = sp->s_inodes_count;
  nblocks = sp->s_blocks_count;
  printf("ninodes = %d nblocks = %d\n", ninodes, nblocks);

  get_block(dev, 2, buf); 
  gp = (GD *)buf;

  bmap = gp->bg_block_bitmap;
  imap = gp->bg_inode_bitmap;
  inode_start = gp->bg_inode_table;
  printf("bmp=%d imap=%d inode_start = %d\n", bmap, imap, inode_start);
  

  init();  
  mount_root();
  //printf("root refCount = %d\n", root->refCount);

  printf("creating P0 as running process\n");
 
  running->status = READY;
  for (int i = 0; i < NPROC; i++)
  {
    proc[i].cwd = iget(dev, 2);
  }
  printf("root refCount = %d\n", root->refCount); //refcount = 3
  printf("mount : %s mounted on / \n", disk);

  while (1)
  {
    //printf("P%d running: \n", running->pid);
    printf("input command : [ls|cd|pwd | mkdir|creat|rmdir | link|unlink|symlink | stat|chmod|utime | open|close|lseek|pfd | read|write|cat|cp|mv | mount|umount|switch| quit] ");
    fgets(line, 128, stdin);
    line[strlen(line) - 1] = 0;

    //Extract the line and strtok by space
    char *token[128], *s;
    char temp[128];
    int n = 0;
    strcpy(temp, line);
    s = strtok(temp, " ");
    while (s)
    {
      token[n++] = s;
      s = strtok(NULL, " ");
    }

    if (line[0] == 0)
      continue;
    pathname[0] = 0;

    sscanf(line, "%s %s", cmd, pathname);
    printf("cmd=%s pathname=%s\n", cmd, pathname);

    if (pathname[0] == '/')
    {
      dev = root->dev;
    }
    else
    {
      dev = running->cwd->dev;
    }

    if (strcmp(cmd, "ls") == 0)
      ls(pathname);
    else if (strcmp(cmd, "cd") == 0)
      chdir(pathname);
    else if (strcmp(cmd, "pwd") == 0)
      pwd(running->cwd);
    else if (strcmp(cmd, "mkdir") == 0)
      make_dir(pathname);
    else if (strcmp(cmd, "creat") == 0)
      creat_file(pathname);
    else if (strcmp(cmd, "rmdir") == 0)
      rmdir(pathname);
    else if (strcmp(cmd, "link") == 0)
      link(token[1], token[2]);
    else if (strcmp(cmd, "unlink") == 0)
      unlink(pathname);
    else if (strcmp(cmd, "symlink") == 0)
      symlink(token[1], token[2]);
    else if (strcmp(cmd, "stat") == 0)
      stat_file(pathname);
    else if (strcmp(cmd, "chmod") == 0)
      chmod_file(token[1], token[2]);
    else if (strcmp(cmd, "utime") == 0)
      utime_file(pathname);
    else if (strcmp(cmd, "open") == 0)
    {
      printf("please provide a mode to open!\nUse 0|1|2|3 for R|W|RW|APPEND\n");
      int mode;
      scanf("%d", &mode);
      while ((getchar()) != '\n');
      fd = open_file(pathname, mode);
    }
    else if (strcmp(cmd, "close") == 0)
    {
      printf("please provide a valid fd to close\n");
      int mode;
      scanf("%d", &mode);
      while ((getchar()) != '\n');
      close_file(mode);
    }
    else if (strcmp(cmd, "lseek") == 0)
    {
      printf("please provide a position to offset\nMust be within bounds of file\n");
      int position;
      scanf("%d", &position);
      while ((getchar()) != '\n');
      my_lseek(fd, position);
    }
    else if (strcmp(cmd, "pfd") == 0)
      pfd();
    else if (strcmp(cmd, "read") == 0)
      read_file();
    else if (strcmp(cmd, "cat") == 0)
      mycat(pathname);
    else if (strcmp(cmd, "write") == 0)
      write_file(pathname);
    else if (strcmp(cmd, "cp") == 0)
      cp(token[1], token[2]);
    else if (strcmp(cmd, "mv") == 0)
      mv(token[1], token[2]);
    else if (strcmp(cmd, "mount") == 0)
    {
      char filesys[BLKSIZE];
      char mount_point[BLKSIZE];
      bzero(filesys, BLKSIZE);
      bzero(mount_point, BLKSIZE);
      printf("please enter a filesys to mount\tAnd a pathname as mount_point\n");
      printf("filesys: ");
      fgets(filesys, BLKSIZE, stdin);
      filesys[strlen(filesys) - 1] = 0;

      printf("Mount_point: ");
      fgets(mount_point, BLKSIZE, stdin);
      mount_point[strlen(mount_point) - 1] = 0;

      mount(filesys, mount_point);
    }
    else if (strcmp(cmd, "umount") == 0)
      umount(pathname);
    else if (strcmp(cmd, "switch") == 0)
      sw();
    else if (strcmp(cmd, "quit") == 0)
      quit();
  }
}

int quit()//exits program but also writes back to the disk
{
  int i;
  MINODE *mip;
  if (running->uid != 0)
  {
    printf("\n\n\tONLY SUPERUSER CAN QUIT!!!\n");
    return 0;
  }
  for (i=0; i<NMINODE; i++){
    mip = &minode[i];
    if (mip->refCount > 0 && mip->dirty)//was in use and changed
      iput(mip);
  }
  exit(0);
}
