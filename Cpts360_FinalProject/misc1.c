/************* lvl1misc.c file **************/

int stat_file(char *pathname)
{

    //get inode of filename into memory
    int ino = getino(pathname);
    MINODE *mip = iget(dev, ino);
    INODE *ip = &mip->INODE;
    
    //copy dev and ino into stat struct
    myst.st_dev = dev;
    myst.st_ino = ino;

    //copy mip->inode fields into stat struct
    myst.st_atime = ip->i_atime;
    myst.st_ctime = ip->i_ctime;
    myst.st_mode = ip->i_mode;
    myst.st_mtime = ip->i_mtime;
    myst.st_nlink = ip->i_links_count;
    myst.st_size = ip->i_size;
    myst.st_uid = ip->i_uid;

    iput(mip);//still call iput to lower use count even though we didnt change anything
}

int chmod_file(char *pathname, int mode)
{
    //get INODE of pathname into memory
    int ino = getino(pathname);
    MINODE *mip = iget(dev, ino);
    printf("\n\nMODE IS\t%d", mip->INODE.i_mode);

    if (mip->INODE.i_mode == mode)
    {
        printf("ERROR!!!!!\n");
        printf("Can't change to same mode\n");
        return 0;
    }
    if (running->uid == 0)//super user
    {
        //printf("Super user always has access\n");
    }
    else if (running->uid != mip->INODE.i_uid)
    {
        printf("ERROR!!!!!\n");
        printf("You Don't have access to %s\n", pathname);
        return 0;
    }
    mip->INODE.i_mode = mode;

    mip->dirty = 1;
    iput(mip);
}

int utime_file(char *pathname)
{
    int ino = getino(pathname);
    if (ino == 0)
    {
        printf("File %s doesn't exist\n", pathname);
    }

    MINODE *mip = iget(dev, ino);
    mip->INODE.i_atime = time(0L);

    mip->dirty = 1;
    iput(mip);
}