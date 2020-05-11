/************* mkdir_creat.c file **************/

/*** Summary ***/
/*
    This function gather all the necessary information and do checks to make sure everything is good
    If everything is good, call mymkdir() which will start the process of making a dir
    Before exiting this function, it increases the link counts, set the time, and mark it dirty
    Because the dir has been touched
*/
int make_dir(char *pathname)
{
    //printf("inside mkdir\n");
    MINODE *start, *pip;
    char *parent, *child;
    char dpath[128], bpath[128];
    int pino;
    //use temps to preserve full pathname as global var
    strncpy(dpath, pathname, strlen(pathname));
    strncpy(bpath, pathname, strlen(pathname));
    dpath[strlen(pathname)] = 0;
    bpath[strlen(pathname)] = 0;

    //printf("dpath = %s\n", dpath);
    //printf("bpath = %s\n", bpath);
    printf("pathname is %s\n", pathname);
    //#1. check if pathname is root
    if (pathname[0] == '/')//root
    {
        //printf("root\n");
        start = root;
        dev = root->dev;
    }
    else//not root
    {
        //printf("not root\n");
        start = running->cwd;
        dev = running->cwd->dev;
    }
    
    //#2. break to dirname and basename
    parent = dirname(dpath);
    child = basename(bpath);
    printf("parent = %s\n", parent);
    printf("child = %s\n", child);

    //#3. Get minode of parent
    pino = getino(parent);
    pip = iget(dev, pino);
    //printf("pino = %d\n", pino);

    if (!maccess(pip, 'w'))//no access
    {
        printf("ERROR!!!!!\nNO WRITE PERMISSION IN PARENT DIR!!!!\n");
        iput(pip);
        return -1;
    }

    /******Verify parent inode is a dir and child doesnt exist within parent******/
    if (!S_ISDIR(pip->INODE.i_mode))//parent is not DIR
    {
        printf("not a dir\n");
        return;
    }
    if (search(pip, child) != 0)//check if child exists in parent DIR
    {
        printf("child exists in parent dir\n");
        return;
    }
    printf("Is a DIR and child doesn't already exist\n");
    
    //#4. Call mymkdir(pip, child);
    mymkdir(pip, child);

    //#5. inc parent inode link count by 1
    pip->INODE.i_links_count++;
    //touch atime
    pip->INODE.i_atime = time(0L);
    //mark as dirty
    pip->dirty = 1;

    //#6. iput(pip);
    iput(pip); 
    
}

/*** Summary ***/
/*
    mymkdir() sets up all the information needed for the dir that is about to be created
    Set the mode, id, link count time, etc...
    Set up the "." // Child
    Set up the ".." // Parent
    Once all that is done, proceed to call enter_name() which will allocate memory in the data block for the DIR
*/
int mymkdir(MINODE *pip, char *name)
{
    //#1. pip points at parent minode[], name is a string
    MINODE *mip;
    int ino, bno;
    //printf("pathname is %s\n", name);

    //#2. allocate an inode and disk block for new directory
    ino = ialloc(dev);//allocate inode # from bitmap
    bno = balloc(dev);//returns free disk block #
 
    //#3. Load inode into a minode[]
    mip = iget(dev, ino);
    INODE *ip = &mip->INODE;

    //#4. Write contents to mip->INODE to make it a DIR INODE
    ip->i_mode = 0x41ED;        //DIR type and permissions
    ip->i_uid = running->uid;   //owner uid
    ip->i_gid = running->gid;   //group id
    ip->i_size = BLKSIZE;       //size in bytes
    ip->i_links_count = 2;      //Links count = 2 because of . and ..
    ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);//sets to current time
    ip->i_blocks = 2;           //LINUX:  Blocks count in 512-byte chunks
    ip->i_block[0] = bno;       //new DIR has one data block
    for (int i = 1; i < 15; i++)
    {
        ip->i_block[i] = 0;
    }
    mip->dirty = 1;             //mark minode as dirty

    //#5. iput(mip) to write the new INODE to the disk
    iput(mip);                  //writes INODE to disk

    //*****create data block for new DIR containing . and .. entries*****
    char buf[BLKSIZE];
    bzero(buf, BLKSIZE);
    get_block(dev, bno, buf);//gets block into buffer
    DIR *dp = (DIR *)buf;
    
    //#6. Write . and .. entries to a buf[] of BLKSIZE

    //make . entry 
    dp->inode = ino;
    dp->rec_len = 12;
    dp->name_len = 1;
    dp->name[0] = '.';

    //advance to next block
    dp = (char *)dp + 12;

    //make .. entry (pip->ino = parent DIR ino)
    dp->inode = pip->ino;
    dp->rec_len = BLKSIZE - 12; //rec_len spans block
    dp->name_len = 2;
    dp->name[0] = dp->name[1] = '.';
    put_block(dev, bno, buf);

    //#7. enter name ENTRY into parent's directory
    enter_name(pip, ino, name); 
}

/*** Summary ***/
/*
    This functions take in all the information about the file/dir ror whatever is trying to be created
    Then allocate memory in the data block for it
    Procedure:
    First we calculate the needed length for our new entry (myname)
    Next we step through all the data blocks. 
    For each block, we get the contents in a buffer and step to the end (last entry) of the block. 
    We keep an int ideal_length updating each iteration and by the end will contain the ideal length of the last entry
    We calculate the remaining length of the block left, to see if we have enough space to enter the name. 
    If remaining length is greater than or equal to the needed length of our entry, we trim previous last entry
    to its ideal length, advnace 1 more time by that length to get to the new end, and insert our entry info.
    Then put the block back to the disk. 
    If remiaing length is less than needed length of our entry, we must allocate a new block and attach it to the parent
    Then we enter our entry as the first in the new block and put the block back to the disk
*/
int enter_name(MINODE *pip, int myino, char *myname)
{
    char buf[BLKSIZE], temp[256];
    bzero(buf, BLKSIZE);
    DIR *dp;
    char *cp;
    //printf("pathname is %s\n", myname);
    //#3. NEED_LENGTH for myname
    int n_len = strlen(myname);
    int need_length = 4 * ((8 + n_len + 3) / 4); //a multiple of 4
    int ideal_length;
    int remain, blk, i;
    //assume 12 direct blocks
    for (i = 0; i < pip->INODE.i_size/BLKSIZE; i++)//gets total size / BLKSIZE for total number of blocks
    {
        if(pip->INODE.i_block[i] == 0)
            break;
        
        //#1. Get parents data block into a buf[]
        get_block(pip->dev, pip->INODE.i_block[i], buf);
        dp = (DIR *)buf;
        cp = buf;

        //#4. Step to last entry block
        blk = pip->INODE.i_block[i];
        printf("step to LAST entry in data block %d\n", blk);
        while (cp + dp->rec_len < buf + BLKSIZE)
        {
            //PRINTS DIR RECORD NAMES WHILE STEPPING THROUGH
            bzero(temp, 256);
            strncpy(temp, dp->name, dp->name_len);
            temp[dp->name_len] = 0;
            printf("[%d %s] ", dp->inode, temp);

            cp += dp->rec_len;
            dp = (DIR *)cp;
            //#2. each entry ideal length
            ideal_length = 4 * ((8 + dp->name_len + 3) / 4); 
        }
        printf("\n");
        //dp now points to last entry in block
        remain = dp->rec_len - ideal_length;

        if (remain >= need_length)//enough space to enter new entry as last
        {
            //enter new entry as last entry in block

            //trim previous entry to ideal length
            dp->rec_len = ideal_length;

            //advance to open spot at end of block
            cp += dp->rec_len;
            dp = (DIR *)cp;

            //setting last entry info
            dp->name_len = strlen(myname);
            strcpy(dp->name, myname);
            dp->name[dp->name_len] = 0;
            dp->rec_len = remain;
            dp->inode = myino;

            put_block(pip->dev, blk, buf);
            return;
        }
    }
   //#5. Not enough space on existing block (allocate new block & attach to parent)
    blk = balloc(dev);              //blk is new allocated block
    pip->INODE.i_block[i] = blk;    //attach blk to parent
    pip->INODE.i_size += BLKSIZE;   //inc parent size by BLKSIZE
    pip->dirty = 1;                 //mark as dirty

    //new block exists now
    get_block(dev, blk, buf);
    dp = (DIR *)buf;
    //entering new entry as first in new block
    //setting entry info
    strcpy(dp->name, myname);
    dp->rec_len = BLKSIZE;
    dp->inode = myino;
    dp->name_len = strlen(myname);

    //#6. Write data block to disk
    put_block(pip->dev, blk, buf);
}

/*** Summary ***/
/*
    creat works exactly the same the mkdir
    However, instead of checking for a DIR to create,
    We make sure that it is a REG file we are trying to create.
*/
int creat_file(char *pathname)
{
    printf("inside creat_file\n");
    MINODE *start, *pip;
    char *parent, *child;
    char dpath[128], bpath[128];
    int pino;
    //use temps to preserve full pathname as global var
    strncpy(dpath, pathname, strlen(pathname));
    strncpy(bpath, pathname, strlen(pathname));
    bpath[strlen(pathname)] = 0;
    dpath[strlen(pathname)] = 0;

    //printf("dpath = %s\n", dpath);
    //printf("bpath = %s\n", bpath);

    //#1. check if pathname is root
    if (pathname[0] == '/') //root
    {
        //printf("root\n");
        start = root;
        dev = root->dev;
    }
    else //not root
    {
        //printf("not root\n");
        start = running->cwd;
        dev = running->cwd->dev;
    }

    //#2. break to dirname and basename
    parent = dirname(dpath);
    child = basename(bpath);
    printf("parent = %s\n", parent);
    printf("child = %s\n", child);

    //#3. Get minode of parent
    pino = getino(parent);
    pip = iget(dev, pino);
    //printf("pino = %d\n", pino);

    if (!maccess(pip, 'w'))
    {
        printf("ERROR!!!!!\nNO WRITE PERMISSION IN PARENT DIR!!!!\n");
        iput(pip);
        return -1;
    }

    /******Verify parent inode is a regular file and child doesnt exist within parent******/
    if (!S_ISDIR(pip->INODE.i_mode)) //parent is not DIR
    {
        printf("parent not a dir\n");
        return;
    }
    if (search(pip, child) != 0) //check if child exists in parent DIR
    {
        printf("child file exists in parent dir\n");
        return;
    }
    printf("parent Is a DIR and child file doesn't already exist\n");

    //#4. Call mymkdir(pip, child);
    my_creat(pip, child);

    //Don't increment parent link count

    //touch atime
    pip->INODE.i_atime = time(0L);
    //mark as dirty
    pip->dirty = 1;

    //#6. iput(pip);
    iput(pip);
}

/*** Summary ***/
/*
    my_creat is the same as mymkdir
    Again, instead of creating a DIR we create a REG file (mode -> 0x81A4)
    Also since we are only creating a file 
    There is no need to allocate a data block, hence no extra "." or ".."
*/
int my_creat(MINODE *pip, char *name)
{
    //#1. pip points at parent minode[], name is a string
    MINODE *mip;
    int ino, bno;

    //#2. allocate an inode and disk block for new directory
    ino = ialloc(dev); //inode
    //bno = balloc(dev); //disk block

    //#3. Load inode into a minode[] to write contents to INODE memory
    mip = iget(dev, ino);
    INODE *ip = &mip->INODE;

    //#4. Write contents to mip->INODE to make it a REG file INODE
    ip->i_mode = 0x81A4;                                //Reg File type and permissions
    ip->i_uid = running->uid;                           //owner uid
    ip->i_gid = running->gid;                           //group id
    ip->i_size = 0;                                     //size in bytes
    ip->i_links_count = 1;                              //Links count = 2 because of . and ..
    ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L); //sets to current time
    mip->dirty = 1;                                     //mark minode as dirty

     //#5. iput(mip) to write the new INODE to the disk
    iput(mip); //writes INODE to disk

    //no need to allocate new block space

    //#7. enter name ENTRY into parent's directory
    enter_name(pip, ino, name);
}
