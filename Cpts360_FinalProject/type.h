/*************** type.h file ************************/

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

typedef struct ext2_super_block SUPER;//info about entire file system
typedef struct ext2_group_desc  GD;
typedef struct ext2_inode       INODE;//each INODE represents a unique file
typedef struct ext2_dir_entry_2 DIR;

SUPER *sp;
GD    *gp;
INODE *ip;
DIR   *dp;   

#define FREE        0
#define READY       1

#define BLKSIZE  1024
#define NMINODE   128
#define NMTABLE    16
#define NFD        16
#define NPROC       2

//in memory inode structure
typedef struct minode{
  INODE INODE;          //disk inode
  int dev, ino;         //device, inode #
  int refCount;         //use count
  int dirty;            //modified flag
  int mounted;          //mounted flag
  struct mounttable *mptr; //mount tbl ptr
}MINODE;

//MOUNT TABLE PTR ONLY USED FOR LVL 3

//used on level 2 
typedef struct oft{
  int  mode;            //mode of opened file
  int  refCount;        //# of procs sharing
  MINODE *mptr;         //ptr to minode of file
  int  offset;          //byte offseet for R|W
}OFT;

//proc structure
typedef struct proc{
  struct proc *next;
  int          pid;     //process id
  int          status;  //status
  int          uid, gid;//user id and group id
  MINODE      *cwd;     //in memory inode of procs cwd
  OFT         *fd[NFD]; //array of file descriptors
}PROC;

//MountTable structure
typedef struct mounttable{
  int dev;            //dev number
  int ninodes;        //# inodes
  int nblocks;        //# blocks
  int bmap;           //block map
  int imap;           //inode map
  int inode_start;    //inode start pt
  MINODE *mptr;  //pointer to mount point MINODE
  char name[64]; //device name
  char mntname[64];//name of where mounted
} MountTable;