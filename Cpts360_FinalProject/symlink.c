/************* symlink.c file **************/
//ALSO READLINK

/*** Summary ***/
/*
    symlink() works like link in a sense, but it is like symlink is a "soft" link 
    whereas link is a "hard" link
    This means that symlink() can link to anything, including DIRs, even files not ton the same device

    Check to make sure old file exists, and new file doesnt already exists
    If passes, create the new file
    Get the ino number of new file as well as the its parent ino number
    set the mode (type) to LNK type (0xA000)
    set the name of the linked file to the data block for later use
    set size of new file to len of old file name
    mark them dirty and iput() them back
*/
int symlink(char *old_file, char *new_file)
{
    // (1) old_file must exist and new_file must not exist
    if (getino(old_file) == 0)
    {
        printf("%s doesn't exist!\n", old_file);
        return 0;
    }
    if (getino(new_file) != 0)
    {
        printf("%s already exists!\n", new_file);
        return 0;
    }

    // (2) creat new_file; change new_file to LNK type;
    creat_file(new_file);

    //get new_file information into MINODE mip
    int ino = getino(new_file);
    MINODE *mip = iget(dev, ino);
    printf("ino = %d\n", ino);

    //get new_file parent information into MINODE pip
    int pino = getino(dirname(new_file));
    MINODE *pip = iget(dev, pino);
    printf("pino = %d\n", pino);

    mip->INODE.i_mode = 0xA000; // change to LNK type

    /*
        (3) Assume length of old_file name <= 60 chars
        store old_file name in newfile's INODE.i_block[] area
        set file size to length of old_file name
        mark new_file's minode dirty;
        iput(new_file's minode);
    */
    //copies contents of "old_file" into i_block
    memcpy(mip->INODE.i_block, old_file, sizeof(old_file));
    //set new file size to # of chars of old_file name
    mip->INODE.i_size = strlen(old_file);
    //mark new_file minode as dirty
    mip->dirty = 1;
    //iput new file minode
    iput(mip);

    /*
        (4) mark new_file parent minode dirty;
        iput(new_file's parent minode);
    */
    //mark new file parent minode as dirty
    pip->dirty = 1;
    //iput new file parent minode
    iput(pip);
}

/*** Summary ***/
/*
    readlink is just trying to get and send the name of the file that is being linked from to buffer
    Verify that file to be read is a LNK type
    memcpy the name of the file linked from from the data block because it was saved there when it was linked
    then return the size 
*/
int readlink(char *name, char buffer[], int number)
{
    isreadlink = true;
    int ino = getino(name);
    if (ino == 0)
        return;
    MINODE *mip = iget(dev, ino);

    //verify LNK type
    if (!S_ISLNK(mip->INODE.i_mode))
    {
        printf("file is not a link type\n");
        return 0; //jump out if not LNK type
    }

    //copy target filename from INODE.iblock[] into buffer
    memcpy(buffer, mip->INODE.i_block, sizeof(mip->INODE.i_block));

    //return file size and set back to false
    isreadlink = false;
    return mip->INODE.i_size;
}