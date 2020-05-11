/*********** util.c file ****************/

int tst_bit(char *buf, int bit)
{
   return buf[bit / 8] & (1 << (bit % 8));
}
int set_bit(char *buf, int bit)
{
   buf[bit / 8] |= (1 << (bit % 8));
}
int clr_bit(char *buf, int bit)//clear bit in char buf[BLKSIZE]
{
   buf[bit / 8] &= ~(1 << (bit % 8));
}
int ialloc(int dev)//allocate an inode number from inode_bitmap
{
   int i;
   char buf[BLKSIZE];

   //read inode_bitmap block
   get_block(dev, imap, buf);
   for (i = 0; i < ninodes; i++)
   {
      if (tst_bit(buf, i) == 0)
      {
         set_bit(buf, i);
         put_block(dev, imap, buf);
         // printf("allocated ino = %d\n", i + 1); //bits count from 0; ino from 1
         return i + 1;
      }
   }
   return 0;
}
int balloc(int dev)//returns a free disk block number
{
   int i;
   char buf[BLKSIZE];

   //read inode_bitmap block
   get_block(dev, bmap, buf);
   for (i = 0; i < nblocks; i++)
   {
      if (tst_bit(buf, i) == 0)
      {
         set_bit(buf, i);
         put_block(dev, bmap, buf);
         // printf("allocated bno = %d\n", i + 1); //bits count from 0; ino from 1
         return i + 1;
      }
   }
   return 0;
}

int idalloc(int dev, int ino)//deallocates an ino number
{
   int i;
   char buf[BLKSIZE];

   if (ino > ninodes)
   {
      printf("inumber %d is out of range\n", ino);
      return 0;
   }

   //get inode bitmap block
   get_block(dev, imap, buf);
   clr_bit(buf, ino - 1);

   //write buf back
   put_block(dev, imap, buf);
}

int bdalloc(int dev, int blk)//deallocate a block number
{
   int i;
   char buf[BLKSIZE];

   if (blk > nblocks)
   {
      printf("blk number %d is out of range\n", blk);
      return 0;
   }

   get_block(dev, bmap, buf);
   clr_bit(buf, blk - 1);

   put_block(dev, bmap, buf);
}

int get_block(int dev, int blk, char *buf)//get info from desired block
{
   //skip forward to desired block
   lseek(dev, (long)blk*BLKSIZE, 0);
   //read desired block
   int n = read(dev, buf, BLKSIZE);
   if (n < 0)
   {
      printf("get_block [%d  %d] error\n", dev, blk);
   }
}   
int put_block(int dev, int blk, char *buf)//put info into desired block
{
   //skip forward to desired block
   lseek(dev, (long)blk*BLKSIZE, 0);
   //write back to desired block 
   int n = write(dev, buf, BLKSIZE);
   if (n != BLKSIZE)
   {
      printf("put_block [%d  %d] error\n", dev, blk);
   }
}   

int tokenize(char *pathname)
{
  int i;
  char *s;
  if (isreadlink == false)
  {
  printf("tokenize %s\n", pathname);
  }
  strcpy(gpath, pathname);   // tokens are in global gpath[ ]
  n = 0;
  s = strtok(gpath, "/");
  while(s){
    name[n] = s;
    n++;
    s = strtok(0, "/");
  }

  if(isreadlink == false)
  {
   for (i= 0; i<n; i++)
      printf("%s  ", name[i]);
   printf("\n");
  }
}

// return minode pointer to loaded INODE
MINODE *iget(int dev, int ino)
{
  int i;
  MINODE *mip;
  char buf[BLKSIZE];
  int blk, offset;
  INODE *ip;

  for (i=0; i<NMINODE; i++){//searching for specific minode (unique dev and ino numbers)
    mip = &minode[i];
    if ((mip->dev == dev) && (mip->ino == ino) && mip->refCount){
       mip->refCount++;
       //printf("found [%d %d] as minode[%d] in core\n", dev, ino, i);
       return mip;
    }
  }
    
  for (i=0; i<NMINODE; i++){//updating a MINODE to show its in use and setting dev, ino.
    mip = &minode[i];
    if (mip->refCount == 0){
       //printf("allocating NEW minode[%d] for [%d %d]\n", i, dev, ino);
       mip->refCount = 1;
       mip->dev = dev;
       mip->ino = ino;

       // get INODE of ino into buf[ ]    
       blk    = (ino-1)/8 + inode_start;
       offset = (ino-1) % 8;

       //printf("iget: ino=%d blk=%d offset=%d\n", ino, blk, offset);

       get_block(dev, blk, buf);
       ip = (INODE *)buf + offset;
       // copy INODE to mp->INODE
       mip->INODE = *ip;
       return mip;
    }
  }   
  printf("PANIC: no more free minodes\n");
  return 0;
}

void iput(MINODE *mip)
{
   int i, block, offset;
   char buf[BLKSIZE];
   INODE *ip;

   mip->refCount--;//lower use count by 1
   if (mip == 0)
      return;
   if (mip->refCount > 0) // minode is still in use
      return;
   if (!mip->dirty)        // INODE has not changed; no need to write back
      return;

   /* write INODE back to disk */
   /***** NOTE *******************************************
   For mountroot, we never MODIFY any loaded INODE
                  so no need to write it back
   FOR LATER WORK: MUST write INODE back to disk if refCount==0 && DIRTY

   Write YOUR code here to write INODE back to disk
   ********************************************************/
   block = (mip->ino - 1) / 8 + inode_start;
   offset = (mip->ino - 1) % 8;

   //get block containing this inode
   get_block(mip->dev, block, buf);
   ip = (INODE *)buf + offset;      //ip points at INODE
   *ip = mip->INODE;                //copy INODE to inode in block
   put_block(mip->dev, block, buf); //write back to disk
   mip->refCount = 0;
}

int search(MINODE *mip, char *name)//returns inode number if found
{
   char *cp, c, sbuf[BLKSIZE], temp[256];
   DIR *dp;
   INODE *ip;
   if (isreadlink == false)
   {
      printf("search for %s in MINODE = [%d, %d]\n", name, mip->dev, mip->ino);
   }
   ip = &(mip->INODE);

   /*** search for name in mip's data blocks: ASSUME i_block[0] ONLY ***/

   get_block(mip->dev, ip->i_block[0], sbuf);
   dp = (DIR *)sbuf;
   cp = sbuf;
   if (isreadlink == false)
   {
      printf("  ino   rlen  nlen  name\n");
   }

   while (cp < sbuf + BLKSIZE)
   {
      strncpy(temp, dp->name, dp->name_len);
      temp[dp->name_len] = 0;//terminating temp with null char
      if (isreadlink == false)
      {
         printf("%4d  %4d  %4d    %s\n", dp->inode, dp->rec_len, dp->name_len, temp);
      }
      if (strcmp(temp, name)==0)
      {
         if (isreadlink == false)
         {
            printf("found %s : ino = %d\n", temp, dp->inode);
         }
         return dp->inode;
      }
      //advance to next data block
      cp += dp->rec_len;
      dp = (DIR *)cp;
   }
   return 0;//if not found
}
/*** Summary ***/
/*
Checks whether current user has proper permission to access DIR under given mode
Same as access above but with mip as parameter
*/
int maccess(MINODE *mip, char mode)
{
   if (running = &proc[0]) //super user
   {
      //printf("SuperUser has access!\n");
      return true;
   }
   //checking owner
   if (mip->INODE.i_mode == S_IRWXU)
   {
      if (mode == 'r')
      {
         if (mip->INODE.i_mode == S_IRUSR) //user has access to read from file
         {
            return true;
         }
      }
      else if (mode == 'w')
      {
         if (mip->INODE.i_mode == S_IWUSR) //user has access to write to file
         {
            return true;
         }
      }
      else if (mode == 'x')
      {
         if (mip->INODE.i_mode == S_IXUSR) //user has access to execute a file
         {
            return true;
         }
      }
   }
   //checking if same group
   else if (mip->INODE.i_mode == S_IRWXG)
   {
      if (mode == 'r')
      {
         if (mip->INODE.i_mode == S_IRGRP) //user has access to read from file
         {
            return true;
         }
      }
      else if (mode == 'w')
      {
         if (mip->INODE.i_mode == S_IWGRP) //user has access to write to file
         {
            return true;
         }
      }
      else if (mode == 'x')
      {
         if (mip->INODE.i_mode == S_IXGRP) //user has access to execute a file
         {
            return true;
         }
      }
   }
   //checking if other bits turned on
   else if (mip->INODE.i_mode == S_IRWXO)
   {
      if (mode == 'r')
      {
         if (mip->INODE.i_mode == S_IROTH) //user has access to read from file
         {
            return true;
         }
      }
      else if (mode == 'w')
      {
         if (mip->INODE.i_mode == S_IWOTH) //user has access to write to file
         {
            return true;
         }
      }
      else if (mode == 'x')
      {
         if (mip->INODE.i_mode == S_IXOTH) //user has access to execute a file
         {
            return true;
         }
      }
   }
   return false; //if at end user doesn't have access
}

/*** Summary ***/
/* 
(3).1. Dwonward traversal: When traversing the pathname /a/b/c/x/y, once you 
reach the minode of /a/b/c, you should see that the minode has been mounted on 
(mounted flag=1). Instead of searching for x in the INODE of /a/b/c, you must

   .Follow the minode's mountTable pointer to locate the mount table entry.
   .From the newfs's dev number, iget() its root (ino=2) INODE into memory.
   .Then,continue to search for x/y under the root INODE of newfs.

(3).2. Upward traversal: Assume that you are at the directory /a/b/c/x/ and 
traversing upward, e.g. cd  ../../,  which will cross the mount point at /a/b/c.

When you reach the root INODE of the mounted file system, you should see that it
is a root directory (ino=2) but its dev number differs from that of the real 
root. Using its dev number, you can locate its mount table entry, which points 
to the mounted minode of /a/b/c/. Then, you switch to the minode of /a/b/c/ and 
continue the upward traversal. Thus, crossing mount point is like a monkey or 
squirrel hoping from one tree to another tree and then back.*/
int getino(char *pathname)
{
   int i, ino, blk, disp;
   char buf[BLKSIZE];
   INODE *ip;
   MINODE *mip, *newmip;

   if (isreadlink == false)
   {
      printf("getino: pathname=%s\n", pathname);
   }
   if (strcmp(pathname, "/")==0)
      return 2;
  
   // starting mip = root OR CWD
   if (pathname[0]=='/')
      mip = root;
   else
      mip = running->cwd;

   mip->refCount++;         // because we iput(mip) later
   
   tokenize(pathname);

   for (i=0; i<n; i++)
   {
      if (isreadlink == false)
      {
         printf("===========================================\n");
         printf("getino: i=%d name[%d]=%s\n", i, i, name[i]);
      }
      if (!maccess(mip, 'x'))//check x bit of current DIR mip
      {
         printf("\nERROR!!!!! NO ACCESS\n");
         iput(mip);
         return 0;
      }
      //up cross mounting point
      if ((strcmp(name[i], "..") == 0) && (mip->dev != root->dev) && (mip->ino == 2))
      {
         printf("UP cross mounting point\n");
         for (int j = 0; j < NMTABLE; j++)
         {
            if (dev == mtable[j].dev)
            {
               newmip = mtable[j].mptr;
               break;
            }
         }
         iput(mip);
         // get parent vvv
         int my_ino, parent_ino, x = 0;
         char *my_name[NMINODE];

         // FROM wd->INODE.i_block[0] get, my_ino & parent_ino
         char buf[BLKSIZE], temp[256];
         DIR *dp;
         char *cp;

         // Assume DIR has only one data block i_block[0]
         get_block(dev, mip->INODE.i_block[0], buf);
         dp = (DIR *)buf;
         cp = buf;

         while (cp < buf + BLKSIZE && x < 2) //while still in data block
         {
            strncpy(temp, dp->name, dp->name_len);
            temp[dp->name_len] = 0;

            if (strcmp(temp, ".") == 0)
            {
               my_ino = dp->inode; //found my_ino number
               x++;
            }
            if (strcmp(temp, "..") == 0)
            {
               parent_ino = dp->inode; //found parent_ino number
               x++;
            }
            // printf("[%d %s]  ", dp->inode, temp); // print [inode# name]

            //advancing to next data block
            cp += dp->rec_len;
            dp = (DIR *)cp;
         }         
         // get parent ^^^
         mip = iget(dev, parent_ino);
         dev = newmip->dev;
         // continue;
      }

      ino = search(mip, name[i]);
      newmip = iget(dev, ino);
      iput(mip);
      mip = newmip;

      if (ino==0){
         iput(mip);
         if (isreadlink == false)
         {
            printf("name %s does not exist\n", name[i]);
         }
         return 0;
      }
      //down cross mounting point
      if (mip->mounted)
      {
         printf("DOWN cross mounting point\n");
         iput(mip);
         dev = mip->mptr->dev;
         mip = iget(dev, 2);
      }
   }

   iput(mip);                   // release mip  
   return ino;
}

int findmyname(MINODE *parent, u32 myino, char *myname) 
{
  // WRITE YOUR code here:
  // search parent's data block for myino;
  // copy its name STRING to myname[ ];
   char buf[BLKSIZE];
   char name[256];

   get_block(dev, parent->INODE.i_block[0], buf);
   dp = (DIR *)buf;
   char *cp = buf;

   while (cp < buf + BLKSIZE)
   {
      // get current dir name
      if (dp->inode == myino)//found
      {
         strncpy(name, dp->name, dp->name_len);
         name[dp->name_len] = 0;
         strcpy(myname, name);
         return 1;
      }

      cp += dp->rec_len; // increment by length of record
      dp = (DIR *)cp;
   }
   return 0;
}

int findino(MINODE *mip, u32 *myino) // myino = ino of . return ino of ..
{
  char buf[BLKSIZE], *cp;   
  DIR *dp;

  get_block(mip->dev, mip->INODE.i_block[0], buf);//get desired block
  cp = buf; 
  dp = (DIR *)buf;
  *myino = dp->inode;
  cp += dp->rec_len;
  dp = (DIR *)cp;
  return dp->inode;//return dp inode number
}

int show(MINODE *pip)//shows the contents of a minode
{
   int i;
   char sbuf[BLKSIZE];
   char c, *cp;
   INODE *ip = &pip->INODE;
   DIR *dp;
   for (i = 0; i < 12; i++)//search direct blocks only
   {
      if (ip->i_block[i] == 0)
         return 0;

      get_block(dev, ip->i_block[i], sbuf);
      dp = (DIR *)sbuf;
      cp = sbuf;
      printf("    i_number  rec_len  name_len    name\n");

      while (cp < sbuf + BLKSIZE)
      {
         c = dp->name[dp->name_len];
         dp->name[dp->name_len] = 0;

         printf("%8d%8d%8u          %s\n", dp->inode, dp->rec_len, dp->name_len, dp->name);

         dp->name[dp->name_len] = c;
         cp += dp->rec_len;
         dp = (DIR *)cp;
      }
   }
}

void display_block_num(MINODE *mip)
{
   int ibuf[256], dbuf[256];
   int buf;
   INODE *ip = &(mip->INODE);

   //print direct blocks
   for (int i = 0; i < 12; i++)
   {
      get_block(dev, ip->i_block[i], buf);
      if (buf)
      {
         printf("%d ", buf);
      }
   }

   if (ip->i_block[12]) //indirect block
   {
      get_block(dev, ip->i_block[12], ibuf); //ibuf[] = [i1,i2..]

      for (int i = 0; i < 256; i++)
      {
         if (ibuf[i])
            printf("%d ", ibuf[i]);
      }
      }

   if (ip->i_block[13])//double indirect block
   {
      get_block(dev, ip->i_block[13], dbuf);//dbuf[] = [d1, d2...]

      for (int i = 0; i < 256; i++)
      {
         if (dbuf[i])
         {
            get_block(dev, dbuf[i], ibuf);

            for (int j = 0; j < 256; j++)
            {
               if (ibuf[j])
               {
                  printf("%d ", ibuf[j]);
               }
            }
         }
      }
   }
}

void delete_blocks(MINODE *mip)//releases all of an inodes data blocks
{
   int ibuf[256], dbuf[256];
   int buf;
   INODE *ip = &(mip->INODE);

   //release direct blocks
   for (int i = 0; i < 12; i++)
   {
      if (ip->i_block[i] == 0)
         continue;
      bdalloc(mip->dev, ip->i_block[i]);
   }

   if (ip->i_block[12]) //release indirect blocks
   {
      get_block(dev, ip->i_block[12], ibuf); //ibuf[] = [i1,i2..]

      for (int i = 0; i < 256; i++)
      {
         if (ibuf[i])
            bdalloc(mip->dev, ibuf[i]);
      }
   }

   if (ip->i_block[13]) //release double indirect block
   {
      get_block(dev, ip->i_block[13], dbuf); //dbuf[] = [d1, d2...]

      for (int i = 0; i < 256; i++)
      {
         if (dbuf[i])
         {
            get_block(dev, dbuf[i], ibuf);

            for (int j = 0; j < 256; j++)
            {
               if (ibuf[j])
               {
                  bdalloc(mip->dev, ibuf[j]);
               }
            }
         }
      }
   }
}


/*** Summary ***/
/*
Checks whether current user has proper permission to access file under given mode
*/
int access(char *pathname, char mode)
{
   int ino;
   MINODE *mip;
   if (running = &proc[0]) //super user
   {
      printf("SuperUser has access!\n");
      return true;
   }
   ino = getino(pathname);
   if (ino <= 0)
   {
      printf("\nERROR ACCESSING FILE!!!!!\n");
      return false;
   }
   mip = iget(dev, ino);

   //checking owner
   if (mip->INODE.i_mode == S_IRWXU)
   {
      if (mode == 'r')
      {
         if (mip->INODE.i_mode == S_IRUSR) //user has access to read from file
         {
            return true;
         }
      }
      else if (mode == 'w')
      {
         if (mip->INODE.i_mode == S_IWUSR) //user has access to write to file
         {
            return true;
         }
      }
      else if (mode == 'x')
      {
         if (mip->INODE.i_mode == S_IXUSR) //user has access to execute a file
         {
            return true;
         }
      }
   }
   //checking if same group
   else if (mip->INODE.i_mode == S_IRWXG)
   {
      if (mode == 'r')
      {
         if (mip->INODE.i_mode == S_IRGRP) //user has access to read from file
         {
            return true;
         }
      }
      else if (mode == 'w')
      {
         if (mip->INODE.i_mode == S_IWGRP) //user has access to write to file
         {
            return true;
         }
      }
      else if (mode == 'x')
      {
         if (mip->INODE.i_mode == S_IXGRP) //user has access to execute a file
         {
            return true;
         }
      }
   }
   //checking if other bits turned on
   else if (mip->INODE.i_mode == S_IRWXO)
   {
      if (mode == 'r')
      {
         if (mip->INODE.i_mode == S_IROTH) //user has access to read from file
         {
            return true;
         }
      }
      else if (mode == 'w')
      {
         if (mip->INODE.i_mode == S_IWOTH) //user has access to write to file
         {
            return true;
         }
      }
      else if (mode == 'x')
      {
         if (mip->INODE.i_mode == S_IXOTH) //user has access to execute a file
         {
            return true;
         }
      }
   }
   return false; //if at end user doesn't have access
}

