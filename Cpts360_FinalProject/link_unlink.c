/************* link_unlink.c file **************/

/*** Summary ***/
/*
    The function link() creates a new file that will be linked to the old file
    First check to make sure the file that is being linked from exists, if not stop.
    It it exists, then make sure that user isn't trying to link from a DIR
    Then make sure, that the file that user is trying to link to doesnt already exists, if yes stop.
    If all checks passed, proceed with the link algorithm
    Get the necessary information (ino number, name of new file trying to link, parent MINODE)
    Then call enter_name() which will allocate a data block for the new file -- Description for enter_name() is above its function
*/
int link(char *old_file, char *new_file)
{
    // verify old_file exists and is not DIR
    int oino = getino(old_file);
    if (oino == 0)
    {
        printf("%s doesn't exist!\n", old_file);
        return 0;
    }
    //file exists
    MINODE *omip = iget(dev, oino);

    if(S_ISDIR(omip->INODE.i_mode))
    {
        printf("CAN'T WORK WITH A DIR DUMMY\n");
        return 0;
    }
    if (getino(new_file) != 0)
    {
        printf("New file already exists! WON'T WORK\n");
        return 0;
    }

    //(3) creat new_file with the same inode number of old_file:
    // creat new entry in new parent DIR with same inode number of old_file

    //temps to preservs full new_file string
    char *parent, *child;

    parent = dirname(new_file);
    child = basename(new_file);

    int pino = getino(parent);
    MINODE *pmip = iget(dev, pino);

    if (!maccess(pmip, 'w'))
    {
        printf("ERROR!!!!!\nNO WRITE PERMISSION IN PARENT DIR!!!!\n");
        iput(pmip);
        return -1;
    }

    enter_name(pmip, oino, child);
    
    //(4)
    omip->INODE.i_links_count++;//inc INODE links_count by 1
    omip->dirty = 1;            //for write back by iput(omip)
    iput(omip);
    iput(pmip);
    
}

/*** Summary ***/
/*
    This function will delete the file from the directory then deallocates the block
    First get ino number of file, make sure that the file exists
    If it does, make sure the parameter is not of type DIR
    Once all the checks passed
    call rmchild() to delete the entry
    we then proceed to deallocate the data block that was created
    earlier when we linked the files (result of enter_name_())
    once all is done, release the MINODE
*/
int unlink(char *filename)
{
    // (1)
    int ino = getino(filename);
    if (ino == 0)
    {
        printf("ERROR!!!!!\n");
        printf("file %s doesn't exist PAL\n", filename);
        return 0;
    }
    MINODE *mip = iget(dev, ino);

    if (S_ISDIR(mip->INODE.i_mode))
    {
        printf("ERROR!!!!!\n");
        printf("CANT WORK WITH DIR BUDDY GUY PAL\n");
        return 0;
    }

    if (running->uid == 0) //super user
    {
        //printf("Super user always has access\n");
    }
    else if (running->uid != 0)
    {
        printf("ERROR!!!!!\n");
        printf("You Don't have access to %s\n", filename);
        return 0;
    }

    // (2)
    char *parent, *child;

    parent = dirname(filename);
    child = basename(filename);

    int pino = getino(parent);
    MINODE *pmip = iget(dev, pino);

    isunlink = true;
    /********************/
    rm_child(pmip, child);
    /********************/
    isunlink = false;

    pmip->dirty = 1;
    iput(pmip);

    // (3)
    mip->INODE.i_links_count--;

    // (4)
    if (mip->INODE.i_links_count > 0)
        mip->dirty = 1; // for write INODE back to disk
    else                //links count = 0
    {
        // remove filename
        // deallocate all data blocks in INODE
        // deallocate INODE;
        for (int i = 0; i < 12; i++)
        {
            if (mip->INODE.i_block[i] == 0)
                continue;
            bdalloc(mip->dev, mip->INODE.i_block[i]);
        }
        idalloc(mip->dev, mip->ino);
    }

    iput(mip); // release mip
}