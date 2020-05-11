/*********** read_cat.c file ****************/

int read_file()
{
    printf("Please provide a fd and nbytes to read\nfd = ");
    int fd; 
    scanf("%d", &fd);
    while ((getchar()) != '\n');

    printf("nbytes = ");
    int nbytes; 
    scanf("%d", &nbytes);
    while ((getchar()) != '\n');

    printf("");

    // Verify that fd is opened for RD or RW
    if (!running->fd[fd])//not open
    {
        printf("%d is not an open file descriptor", fd);
        return;
    }
    if (running->fd[fd]->mode != 0 && running->fd[fd]->mode != 2)//not RD or RW
    {
        printf("%d is not opened for a valid mode\n", fd);
        return;
    }

    char buf[BLKSIZE];
    return (myread(fd, buf, nbytes));
}


int myread(int fd, char *buf, int nbytes)
{
    //printf("fd ======= %d\n", fd);
    //printf("nbytes =====%d\n", nbytes);

    // (1)
    OFT *oftp;
    oftp = running->fd[fd];
    MINODE *mip;
    mip = oftp->mptr;

    int remain = 0;//number of bytes remaining in readbuf
    int count = 0; //bytes read per function call
    int lbk;//logical block number
    int blk;//physical block number
    int dblk;//help w/ converting double indirect blocks
    int startbyte;//starting byte to read from
    int avil;//number of bytes left to be read from the file
    int max = 0;//max number of bytes read per function call
    int filesize = 0;//size of file to be read
    char readbuf[BLKSIZE];//buffer we read into
    int buf2[256];//for help with indirects
    int buf3[256];//for help with double indirects
    int dbuf[256]; //help converting double indirect blocks
    bzero(readbuf, BLKSIZE);

    char *cq = buf;
    filesize = mip->INODE.i_size;

    avil = filesize - oftp->offset;
    //printf("*************FILESIZE IS %d************", filesize);

    // (2)
    while(nbytes && avil)
    {
        // Compute LOGICAL BLOCK number lbk and startByte in that block from offset
        lbk = oftp->offset/BLKSIZE;
        startbyte = oftp->offset%BLKSIZE;

        if (lbk < 12)//lbk is direct block
        {
            blk = mip->INODE.i_block[lbk];//map logical block lbk to physical block blk
            // printf("lbk = %d  Iblk = %d  blk = %d\n", lbk, mip->INODE.i_block[lbk], blk);
        }
        else if (lbk >= 12 && lbk < 256 + 12)//indirect blocks
        {
            // printf("\nINDIRECT\n");
            get_block(mip->dev, mip->INODE.i_block[12], buf2);
            blk = buf2[lbk - 12]; // get the blk number
            // printf("lbk = %d  Iblk = %d  blk = %d\n", lbk, mip->INODE.i_block[12], blk);
        }
        else // double indirect blocks -> at i_block[13]
        {
            // printf("DOUBLE INDIRECT\n");
            get_block(mip->dev, mip->INODE.i_block[13], buf3);
            lbk -= (12 + 256);
            dblk = buf3[lbk / 256];

            get_block(mip->dev, dblk, dbuf);
            blk = dbuf[lbk % 256];
            // printf("lbk = %d  Iblk = %d  blk = %d\n", lbk, mip->INODE.i_block[13], blk);
        }

        // get the data block into readbuf[BLKSIZE]
        get_block(mip->dev, blk, readbuf);

        // copy from start to buf[], at most remain bytes in this block
        char *cp = readbuf + startbyte;
        remain = BLKSIZE - startbyte;

        //finding min of (nbytes, avil, remain) to set as max # of bytes to be read
        max = remain;
        if (max > nbytes)
        {
            max = nbytes;
        }
        else if (max > avil)
        {
            max = avil;
        }
       
        memcpy(cq, cp, max);
        // printf("MAX IS %d\n", max);
        avil -= max;
        nbytes -= max;
        remain -= max;
        count += max;
        oftp->offset += max;
        
        
        // if one data block is not enough, loop back to OUTER while for more
    }
    // printf("\nmyread: read %d char from file descriptor %d\n", count, fd);
    // printf("There are %d bytes remaining to be read\n", avil);
    return count; //actual number of bytes read
}

int mycat(char *pathname)
{
    int n, fd;
    char buf[BLKSIZE];

    fd = open_file(pathname, 0); // open file for READ
    if (fd < 0)
    {
        printf("cat: file open error\n");
        return (-1);
    }
    while ((n = myread(fd, buf, BLKSIZE)) > 0)
    {
        buf[n] = 0;
        puts(buf);
    }

    printf("\n");
    close_file(fd);
    return(0);
}