/************* rmdir.c file **************/

/*** Summary ***/
/*
    rm_child is the function that deletes the data in the data block
    It is supposed to check for 3 cases
    File to be deleted is last entry
    File to be deleted is middle entry
    File to be deleted is first or only entry
    But since directories that were created has "." and ".."
    The file to be delete can never be the first or the only entry
    So we only have to worry about the last and middle entry to be deleted

    First, loop through the data block until we find the file that we are trying to remove

    CASE 1: Last entry
    All we need to do here is just have a ptr1 to the data block that is about to be removed
    and another ptr2 to data block to the left of it.
    Once we have all that, to remove the entry we just set the rec_len of ptr2 to the rec_len of ptr1
    Then we put the block back to disk

    CASE 2: Middle entry
    This is a little bit trickier
    We must know where the block of the entry that being deleted starts and ends
    And also the length of the rest of the blocks after the entry
    In order to do this, we go through the data block, get all the information we need
    Then change the rec_len of the data block right after the entry 
    by adding the rec_len of itself to the rec_len of the entry

    VISUALIZATION:
    These are blocks of data
    [1-3][4-10][11-15] and say we are trying the delete the block [4-10]
    We save 4 as the start
    We save 10 as the end
    We save 11 as the rest of the length (4-15) -- size
    we delete the entry/ move the right block left by
    memcpy(start, end, size)
    Then we will put the block back to the disk
    The new data block will look like this
    [1-3][4-15]
*/
int rm_child(MINODE *parent, char *name)
{
    char buf[BLKSIZE], temp[256];
    DIR *dp;//tracks current entry
    DIR *pdp;//tracks previous entry
    DIR *newdp, *newpdp;
    char *newcp;
    char *cp;          //helps advance through block
    char  *start, *end;//start and end ptrs for memory copy
    int ideal_length = 0;//helps check if last entry in block or not
    int deleted_entry_length = 0;//rec_len of removed entry
    int size = 0;//size for memory copy

    get_block(parent->dev, parent->INODE.i_block[0], buf);
    dp = (DIR *)buf;
    cp = buf;
    
    //#1. Search parent INODE data block for entry of name
    while (cp < buf + BLKSIZE)
    {
        ideal_length = 4 * ((8 + dp->name_len + 3) / 4);
        //printf("ideal length is %d\n", ideal_length);

        bzero(temp, 256);
        strncpy(temp, dp->name, dp->name_len);
        temp[dp->name_len] = 0;

        printf("temp = %s\n", temp);
        printf("name = %s\n", name);
        
        //#2. Erase name entry from parent directory
        if (strcmp(temp, name) == 0)//found entry to delete
        {
            // Erase name entry from parent directory
            if (isunlink == true)
            {
                printf("TRYING TO REMOVE FILE!!!\n");
            }
            else
            {
                printf("TRYING TO REMOVE DIR!!!!\n");
            }

            printf("ideal length is : %d\n", ideal_length);

            if (dp->rec_len > ideal_length)//WORKING
            {
                if (isunlink == true)
                {
                    printf("FOUND! file is last entry in block\n");
                }
                else
                {
                    printf(" FOUND! dir is last entry in block\n");
                }

                //DIR/FILE to remove is the last one in block
                //add dp rec length to prev dir rec length
                printf("rec_len of removed entry is: %d\n", dp->rec_len);
                printf("rec_len of old 2nd to last entry is %d\n", pdp->rec_len);
                pdp->rec_len += dp->rec_len;
                printf("new rec_len for new last entry in block is %d\n", pdp->rec_len);

                //#3. Write parent's data block back to disk
                put_block(parent->dev, parent->INODE.i_block[0], buf);
                if (isunlink == true)
                {
                    printf("FILE successfully removed\n");
                }
                else
                {
                    printf("Dir successfully removed\n");
                }
                return 0; //successful exit
            }
            else//WORKING finally
            {
                if (isunlink == true)
                {
                    printf("FOUND! FILE in middle of block");
                }
                else
                {
                    printf(" FOUND! DIR in middle of block\n");
                }

                //DIR to remove is in middle of block
                deleted_entry_length = dp->rec_len;//size of deleted entry
                printf("deleted entry length is %d\n", deleted_entry_length);

                start = cp;//gets memcpy destination
                end = (cp + dp->rec_len);//gets memcpy src
                size = buf + BLKSIZE - start;//gets size for memcpy
                //printf("start = %d\n", start);
                //printf("end = %d\n", end);
                //printf("size = %d\n", size);

                        
                //need to step through the rest of block to add rec_len to end
                newdp = (DIR *)start;
                newcp = start;
                while (newcp < buf + BLKSIZE)
                {
                    newpdp = newdp;
                    newcp += newdp->rec_len;
                    newdp = (DIR *)newcp;   //advance dp
                }//newpdp pts to last entry in block
                printf("current last rec_len = %d\n", newpdp->rec_len);
                printf("deleted entry length is %d\n", deleted_entry_length);

                //add len of deleted to current last in block
                newpdp->rec_len += deleted_entry_length;
                printf("new last rec length is %d\n", newpdp->rec_len);

                //move contents to the left
                memcpy(start, end, size);
                show(parent);

                //#3. Write parent's data block back to disk
                put_block(parent->dev, parent->INODE.i_block[0], buf);
                if (isunlink == true)
                {
                    printf("FILE successfully removed\n");
                }
                else
                {
                    printf("Dir successfully removed\n");
                }
                return 0; //exit function after successful deletion
            }
            //mark parent minode as dirty for write back
            parent->dirty++;
            iput(parent);
        }
        pdp = dp;//maintaining prev entry ptr
        //advancing to next entry in block
        cp += dp->rec_len;
        dp = (DIR *)cp;//maintaining current entry ptr
    }
    if (isunlink == true)
    {
        printf("FILE NOT FOUND!! can't be removed GUY!\n");
    }
    else
    {
        printf("DIR NOT FOUND! Can't be removed friend\n");
    }
    return -1;
}

/*** Summary ***/
/*
    This function will deallocate the memory of the dir being removed
    Then it will call rm_child() to actually get rid of the actual item
    Make sure DIR being removed actually exists
    Make sure it not in use, empty, or actually not a DIR at all :|
    If all that passes, deallocate the memory 
    Get ino of thing being removed
    Call rm_child() on it
*/
int rmdir(char *pathname)
{
    // #2. Get inumber of pathname
    int ino = getino(pathname);
    //printf("ino # = %d\n", ino);

    // #3. Get its Minode[] ptr
    MINODE *mip = iget(dev, ino);
    show(mip);//prints mip contents
    printf("%d = refcount\n", mip->refCount);

    // #4. Check ownership (ONLY FOR LVL 3)
    /*  
        Check owndership 
        super user : OK
        not super user : uid must match
    */
    if (running->uid == 0)//super user
    {
        //printf("super user always has access\n");
    }
    else if (running->uid != 0)
    {
        printf("ERROR!!!!!\n");
        printf("\tYou don't have access to remove this DIR");
        return;
    }

    // #5. CHECK dir type, not busy, empty
    //initialize booleans as false
    bool isDir = false;
    bool isNotBusy = false;
    bool isEmpty = false;
 
    if (S_ISDIR(mip->INODE.i_mode))//pathname is a DIR
    {
        isDir = true;
        //printf("is a dir\n");
    }
    if (mip->refCount = 1)
    {
        isNotBusy = true;
        //printf("is not busy\n");
    }
    
    if (mip->INODE.i_links_count <= 2)//pathname contains 2 or less links
    {
        isEmpty = true;

        if (mip->INODE.i_links_count == 2)//if 2 links check only . and ..
        {
            char buf[BLKSIZE];
            DIR *dp;
            char *cp;
            int count = 0;//counts # of items in block

            //read in block to check contents
            get_block(mip->dev, mip->INODE.i_block[0], buf);
            dp = (DIR *)buf;
            cp = buf;

            while (cp < buf + BLKSIZE)//while not at end
            {
                if (count == 3)//more than . and .. entries only
                {
                    isEmpty = false;
                    //printf("too many entries\n");
                }
                count++;

                //advance to next data item in block
                cp += dp->rec_len;
                dp = (DIR *)cp;
            }
        }
    }


    if (!isDir || !isNotBusy || !isEmpty)
    {
        //print error, put back, jump out
        printf("either not DIR, BUSY, or not EMPTY\n");
        iput(mip);
        return -1;
    }
    printf("checks have passed\n");

    
    // #6. Assume checks passed
    //deallocate block and inode
    for (int i=0; i<12; i++)
    {
        if (mip->INODE.i_block[i]==0)
            continue;
        bdalloc(mip->dev, mip->INODE.i_block[i]);
    }
    idalloc(mip->dev, mip->ino);
    iput(mip); // (which clears mip->refCount = 0);
    //printf("%d = refcount\n", mip->refCount);

    
    char temp[256], temp2[256];
    char *parent;
    int parent_ino;
    // #7. get parent DIR's ino and Minode (pointed by pip)

    //returns parents ino
    strncpy(temp2, pathname, strlen(pathname));
    temp2[strlen(pathname)] = 0;
    parent = dirname(temp2);

    parent_ino = getino(parent);
    printf("parent ino = %d\n", parent_ino);
    MINODE *pip = iget(mip->dev, parent_ino);
    //show(pip);
    
    // #8. remove childs entry from parent directory
    findmyname(pip, ino, temp);//entry to remove name copied to temp var
    printf("name to remove is %s\n", temp);
    
    rm_child(pip, temp);
    //pip-> parent Minode, temp = entry to remove

    /*
        #9. decrement pip's link_count by 1; 
        touch pip's atime, mtime fields;
        mark pip dirty;
        iput(pip);
        return SUCCESS;
    */
    pip->INODE.i_links_count--;
    pip->INODE.i_atime = pip->INODE.i_mtime = time(0L); //sets to current time
    pip->dirty = 1;
    iput(pip);
    return 1;//for success 
}
