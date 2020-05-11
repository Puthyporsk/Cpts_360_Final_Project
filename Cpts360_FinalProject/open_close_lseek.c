/*********** open_close_lseek.c file ****************/

int oft_index = 0;//tracks which index for global open file table
int open_file(char *filename, int mode)
{
    char ch;
    //check if root or not
    if (filename[0] == '/')
        dev = root->dev;
    else
        dev = running->cwd->dev;

    //get ino and check if exists
    int ino = getino(filename);
    if (ino == 0)//doesnt exist
    {
        creat_file(filename);//create
        ino = getino(filename);//get new ino
    }

    //get file into minode
    MINODE *mip;
    mip = iget(dev, ino);
    
    if (mode == 0)//converting to char to check access
    {
        ch = 'r';// 'r' for R
    }
    else
    {
        ch = 'w';// 'w' for W, RW, APPEND
    }

    if(!maccess(mip, ch))//no access to open file
    {
        printf("ERROR!!!!!\n YOU DONT HAVE ACCESS TO THIS FILE\n");
        iput(mip);
        return -1;
    }

    //check if regular
    if (!S_ISREG(mip->INODE.i_mode))
    {
        printf("\n\tERROR!!!!!\n");
        printf("Not regular file!\n");
        return;
    }

    //CREATING NEW OFT ENTRY
    OFT *oftp;
    oftp = malloc(sizeof(OFT));//allocating space
    oftp = &oft[oft_index];//pointing to open table oft entry

    //setting oftp info
    oftp->mode = mode;
    oftp->refCount = 1;
    oftp->mptr = mip;

    //check if file already opened
    if (mip->refCount > 1)
    {
        if (mip->dirty == 1)//if file already opened for other than read
        {
            printf("\n\tERROR!!!!!\n");
            printf("File already opened in mode other than read\n");
            return;
        }
        //check if trying to open again for other than read
        if (mode != 0)
        {
            printf("\n\tERROR!!!!!\n");
            printf("file in use already, can only read from open files\n");
            return;
        }
    }

    if (mode == 1) //write. must trunc file size to 0 and set offset to beginning
    {
        //truncate(mip);
        oftp->offset = 0;
    }
    else if (mode == 3)//append mode. set offset to end of file
    {
        oftp->offset = mip->INODE.i_size;
    }
    else//other modes offset at beginning
    {
        oftp->offset = 0;
    }

    int i;
    for (i = 0; i < NFD; i++)//searching through oft entries for lowest
    {
        if (!running->fd[i])
        {
            running->fd[i] = oftp; // Set running fd to OFT entry
            break;
        }
    }
    if (i == NFD)//no free fd left
    {
        printf("\n\tERROR!!!!!\n");
        printf("no remaining file descriptors available for use\n");
        return;
    }

    mip->INODE.i_atime = time(0L); // Touch atime for all
    if (mode != 0)                 // Touch mtime for everything else but READ mode
    {
        mip->INODE.i_mtime = time(0L);
        //mark as dirty for write back
        mip->dirty = 1;
    }

    //doesnt mark as dirty for read

    oft_index++; //after opening, index must be inc
    printf("fd ===== %d\n", i);
    return i;
}

int truncate (MINODE *mip)
{
    //releases all mip->INODES data blocks
    delete_blocks(mip);
    //updates time
    mip->INODE.i_atime = mip->INODE.i_mtime = time(0L);
    mip->INODE.i_size = 0;
    //mark as dirty for write back
    mip->dirty = 1;
    //iput(mip);
}

int close_file(int fd)
{
    //#1. verify fd is in range
    printf("fd = %d\n", fd);
    if (fd >= NFD)
    {
        printf("\n\tERROR!!!!!\n");
        printf("fd %d is out of range\n", fd);
        return;
    }

    //#2. verify running->fd[fd] points at an OTF entry
    if (running->fd[fd] == 0) //not valid OFT
    {
        printf("\n\tERROR!!!!!\n");
        printf("fd %d is not pointing to an oft entry\n", fd);
        return;
    }

    //#3
    OFT *oftp;
    oftp = running->fd[fd];
    running->fd[fd] = 0;
    oftp->refCount--;
    oft_index--;

    if (oftp->refCount > 0)
    {
        printf("\n\tNot last use of this file\n");
        return;
    }
    
    //last user of this OFT entry ==> dispose of MINODE[]
    //printf("\n\tLast user of file!\tWill dispose minode\n");
    MINODE *mip;
    mip = oftp->mptr;
    iput(mip);
}

int my_lseek(int fd, int position)
{
    //record original position
    int op = running->fd[fd]->offset;

    //checking position is within bounds of file
    if (position < 0 || position >= running->fd[fd]->mptr->INODE.i_size)
    {
        printf("lseek out of bounds of file\n");
        printf("range is from 0 to %d", running->fd[fd]->mptr->INODE.i_size);
        return -1;
    }
    //setting offset to new position
    running->fd[fd]->offset = position;

    return op;
}

int pfd()
{
    for (int i = 0; i < NFD; i++) //loop through all possible open file descriptors
    {
        if (running->fd[i]) //file descriptor valid
        {//print descriptions of each file descriptor contents
            printf("\tfd \tmode \toffest \tINODE\n");
            printf("\t%d \t%d   \t%d     \t[dev:%d , ino:%d]\n", i, running->fd[i]->mode, running->fd[i]->offset, running->fd[i]->mptr->dev, running->fd[i]->mptr->ino);
        }
    }
}

int dup(int fd)
{
    int i = 0;
    //   verify fd is an opened descriptor;
    if (!running->fd[fd])
    {
        printf("ERROR!!!!!\n");
        printf("file descriptor not opened\n");
        return;
    }
    //   duplicates (copy) fd[fd] into FIRST empty fd[ ] slot;
    for (; i < NFD; ++i)
    {
        if (running->fd[i])
        {
            continue;
        }
        //i will be first open fd now
        running->fd[i] = running->fd[fd];
        printf("fd %d copied to fd %d", fd, i);
        break;
    }
    //   increment OFT's refCount by 1;
    running->fd[i]++;
}

int dup2(int fd, int gd)
{
    //   CLOSE gd fisrt if it's already opened;
    if (running->fd[gd])
    {
        close_file(gd);
    }
    //   duplicates fd[fd] into fd[gd];
    running->fd[gd] = running->fd[fd];
}