/************* mount_umount.c file **************/

/*** Summary ***/
/*
If the user didn't supply parameters, I display all current mounted systems
from the global Mtable array. Next, I run through the Mtable array to check if 
the filesys is already mounted. If it is, I return with an error, if not, I allocate 
a new entry in the Mtable array for our filesys were trying to mount. Next, I open
the filesys for Read/Write under linux and check to make sure it's an EXT2 filesystem.
I load the mntpoint dir into a MINODE and check it is a DIR and not currently busy. Then, 
I set the mountTable entry info through a local mountptr pointer (including name, dev, etc..)
Finally, I mark the minode for mntpoint as mounted on (= 1) and point it to the Mtable entry.
*/

int mount(char *filesys, char *mntpoint)
{
    ismount = true;
    MountTable *mountptr;
    SUPER *ssp;
    MINODE *mip;
    GD *ggp;
    int ino;
    char buf[BLKSIZE];

    if (running->uid != 0)
    {
        printf("ERROR!!!!!\n");
        printf("ONLY SUPERUSER CAN MOUNT A NEW FILESYS\n");
        return;
    }

    // (1)
    // when missing required parameters
    if (strlen(filesys) == 0 || strlen(mntpoint) == 0)
    {
        // display current mounted filesystem
        printf("\nDisplaying current mounted filesystems:\n\n");
        for (int i = 0; i < NMTABLE; i++) //loop thru table entries
        {
            if (mtable[i].dev) //valid entry
            {
                printf("%s is mounted on %s\n", mtable[i].name, mtable[i].mntname);
            }
        }
        return;
    }

    // (2)
    // check whether filesys is already mounted
    for (int i = 0; i < NMTABLE; i++)
    {
        if (mtable[i].dev)//current entry is mounted
        {
            if (!strcmp(mtable[i].name, filesys))//entry has same name
            {
                printf("\tError\nFilesystem already mounted\n\n");
                return;//reject
            }
        }
    }
    //allocate first free mount table entry
    for (int i = 0; i < NMTABLE; i++)
    {
        if (!mtable[i].dev)
        {
            mountptr = &mtable[i];
            break;
        }
    }


    // (3)
    // Open filesys for RW(under linux); use its fd number as the new DEV
    printf("checking EXT2 FS ....");
    int newDEV;
    if ((newDEV = open(filesys, O_RDWR)) < 0) // 2 for RW
    {
        printf("open %s failed\n", filesys);
        return;
    }


    /********** read super block  ****************/
    get_block(newDEV, 1, buf);
    ssp = (SUPER *)buf;

    /* verify it's an ext2 file system ***********/
    if (ssp->s_magic != 0xEF53)
    {
        printf("magic = %x is not an ext2 filesystem\n", ssp->s_magic);
        return;
    }
    printf("EXT2 FS OK\n");

    // (4)
    // Find ino, then get minode for mount_point
    ino = getino(mntpoint);
    if (ino == 0)
    {
        printf("\tERROR!!!!!\n");
        printf("MountPoint %s doesn't exist\n", mntpoint);
        return;
    }

    mip = iget(dev, ino);

    // (5)
    if (!S_ISDIR(mip->INODE.i_mode)) // Check for DIR type
    {
        printf("\tERROR!!!!!\n");
        printf("%s is not a dir\n", mntpoint);
        return;
    }
    if (mip->refCount > 1) // Check for not BUSY
    {//1 and not 0 because we just called iget above which incs refcount
        printf("\tERROR!!!!!\n");
        printf("%s is in use still as another procs cwd\n", mntpoint);
        return;
    }

    // (6)
    //Record new dEV in the MOUNT table entry
    //also for convenience add a bunch of other stuff
    //mountptr will update our global mtable through pointers
    mountptr->mptr = mip;
    mountptr->dev = newDEV;
    mountptr->ninodes = ssp->s_inodes_count;
    mountptr->nblocks = ssp->s_blocks_count;
    strcpy(mountptr->name, filesys);
    strcpy(mountptr->mntname, mntpoint);

    // (7)
    //Mark mount_point's minode as being mount ::::: ----> mip->mounted = 1
    //let it point to mount table entry
    mip->mounted = 1;
    mip->mptr = mountptr;
    mip->dirty = 1;

    ismount = false;
    return 0; // SUCCESS
}

/*** Summary ***/
/*
First, I search through the global Mtable array to check that the user provided a filesys that is
currently a mounted file system. I point a local Mounttable entry (mnt) at the Mtable entry in the 
array, returning an error if the filesys isn't currently mounted. I then make sure the mnt->dev isn't
the same as the cwd->dev. If so, return an error. I also ran through all the open files to make sure 
none of them belong to our filesys (same dev number). If any do, I return with an error. Lastly, I reset
the mount_points INODE dev to 0 and mounted flag to 0 before calling iput and return 0 for success
*/
int umount(char *filesys)
{
    MountTable *mnt;
    bool ismounted = false;
    int ino;
    MINODE *mip;

    if (running->uid != 0)
    {
        printf("ERROR!!!!!\n");
        printf("ONLY SUPERUSER CAN UNMOUNT A FILESYS\n");
        return;
    }

    // (1)
    // Search mnttable to check filesys is indeed mounted
    for (int i = 0; i < NMTABLE; i++)
    {
       if (mtable[i].dev)//entry exists
       {
           if (!strcmp(mtable[i].name, filesys)) // Found the FS with the same name
           {
               mnt = &mtable[i]; //point local instance at our targeted filesystem
               ismounted = true;
               printf("filesys %s to be unmounted is currently mounted on %s\n\n", mnt->name, mnt->mntname);
               break;
           }
       }
    }
    if (ismounted == false)//filesystem not mounted
    {
        printf("\tERROR!!!!!\n");
        printf("%s is not an already mounted filesystem\n", filesys);
        return;
    }

    // (2)
    // check if current mounted fs is BUSY -> cant be unmounted
    if (running->cwd->dev == mnt->dev)//filesys is our cwd
    {
        printf("\tERROR!!!!!\n");
        printf("%s is still active and can't be unmounted\n", filesys);
        return;
    }
    //checking all open files to see if they belong to our filesys
    for (int i = 0; i < NFD; i++)//checking all open files
    {
        if (running->fd[i])//fd contains opened file
        {
            if (running->fd[i]->mptr->dev == mnt->dev)//opened file is in our filesys (same dev number)
            {
                printf("\tERROR!!!!!\n");
                printf("%s has opened files still!!\n", filesys);
                return;
            }
        }
    }
    
    // (3)
    // Find mount_point's inode (should be in memory while mounted on)
    //reset to "not mounted" then iput the minode
    mip = mnt->mptr;
    mnt->dev = 0;
    mnt->mptr->mounted = 0;
    iput(mip);
    printf("%s is no longer mounted\n", filesys);
    return 0; // SUCCESS
}

/*** Summary ***/
/*
Switches the current running process from p0 to p1 or from p1 to p0
*/
int sw()
{
    printf("Switching processes now!!!!\n");
    if (running->uid == 0)
    {
        printf("\n\tP1 IS NEW RUNNING PROC\n");
        running->uid = 1;
    }
    else
    {
        printf("\n\tP0 IS NEW RUNNING PROC\n");
        running->uid = 0;
    }
}

