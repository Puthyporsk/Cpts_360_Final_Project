/************* write_cp.c file **************/

int write_file(char *pathname)
{
    printf("Please provide a fd and a text string to write\nfd = ");
    int fd; 
    scanf("%d", &fd);
    while ((getchar()) != '\n');

    printf("text string = ");
    char txt[BLKSIZE];
    fgets(txt, 128, stdin);
    txt[strlen(txt) - 1] = 0;

    int nbytes = strlen(txt);
    printf("buf ==== %s\tnbytes = %d\n", txt, nbytes);

    return (mywrite(fd, txt, nbytes));
}

int mywrite(int fd, char *buf, int nbytes)
{
    OFT *oftp;
    oftp = running->fd[fd];
    MINODE *mip;
    mip = oftp->mptr;

    if (oftp->mode == 0)
    {
        printf("open for read, not valid writing mode\n");
        return;
    }

    int count = 0, lbk, blk, start, ibuf[256], remain, max = 0;
    char wbuf[BLKSIZE], *cp;

    char *cq = buf;
    int nb = nbytes;
    while (nbytes > 0)
    {
        lbk =  oftp->offset/BLKSIZE;
        start = oftp->offset%BLKSIZE;
        
        // printf("lbk = %d\n", lbk);

        if (lbk < 12)
        {
            if (mip->INODE.i_block[lbk] == 0)
            {
                mip->INODE.i_block[lbk] = balloc(mip->dev);
            }
            blk = mip->INODE.i_block[lbk];
        }
        else if (lbk >= 12 && lbk < 256 + 12)
        {
            //  Indirect block
            if (mip->INODE.i_block[12] == 0)
            {
                mip->INODE.i_block[12] = balloc(mip->dev); // allocate
                // zero out the block
                get_block(mip->dev, mip->INODE.i_block[12], wbuf);
                for(int i = 0; i < BLKSIZE; i++)
                    wbuf[i] = 0;
                put_block(mip->dev, mip->INODE.i_block[12], wbuf);
            }

            get_block(mip->dev, mip->INODE.i_block[12], ibuf); // get i_block[12] into an int ibuf[256]
            
            blk = ibuf[lbk-12];

            if (blk == 0)
            {
                *ibuf = balloc(mip->dev);
                blk = *ibuf;
            }
        }
        else
        {
            if (mip->INODE.i_block[13] == 0)
            {
                mip->INODE.i_block[13] = balloc(mip->dev); // allocate
                // zero out the block
                get_block(mip->dev, mip->INODE.i_block[13], wbuf);
                for(int i = 0; i < BLKSIZE; i++)
                    wbuf[i] = 0;
                put_block(mip->dev, mip->INODE.i_block[13], wbuf);
            }
            // Double indirect blocks.
            // double indirect blocks -> at i_block[13]
            get_block(mip->dev, mip->INODE.i_block[13], (char*)buf);
            lbk -= (12+256);

            get_block(mip->dev, buf+(lbk/256), buf);
            blk = buf + (lbk%256);
        }

        // all cases come to here : write to the data block
        get_block(mip->dev, blk, wbuf); // read disk block into wbuf[]
        cp = wbuf + start; // cp points at start in wbuf[]
        remain = BLKSIZE - start; // number of BYTEs remain in this block
        
        //finding max bytes to be read in 1 go
        max = remain;
        if (max > nbytes)
        {
            max = nbytes;
        }
        memcpy(cq, cp, max);
        remain -= max;
        nbytes -= max;
        oftp->offset += max;
        if (oftp->offset > mip->INODE.i_size) // especially for RW|APPEND mode
            mip->INODE.i_size+= max; // inc file size
        put_block(mip->dev, blk, wbuf); // write wbuf[] to disk
        // loop back to outer while to write more ... until nbytes are written
    }
    mip->dirty = 1;
    // printf("wrote %d char into file descriptor fd = %d\n", nb, fd);
    return nb;
}

int cp(char *src, char *dest)
{
    int fd = open_file(src, 0);
    int gd = open_file(dest, 1);
    printf("fd = %d\n", fd);
    printf("gd = %d\n", gd);

    int n = 0;
    char buf[BLKSIZE];
    while (n = myread(fd, buf, BLKSIZE))
    {
        mywrite(gd, buf, n);
    }
}

int mv(char *src, char *dest)
{
    int ino = getino(src);
    if (ino == 0)
    {
        printf("file %s does not exist!\n", src);
        return;
    }

    MINODE *mip;
    mip = iget(dev, ino);

    // Check to see whether src is on the same dev as src
    if (mip->dev == dev) // CASE 1: same dev
    {
        link(src, dest);
        unlink(src);
    }   
    else
    {
        cp(src, dest);
        unlink(src);
    }
}