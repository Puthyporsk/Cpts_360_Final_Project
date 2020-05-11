/************* cd_ls_pwd.c file **************/

/*** Summary ***/
/*
    For this function, we have 2 cases
    When user cd with a parameter and one without
    When user cd without a parameter, that means the user is trying to cd to the root
    so just change the cwd to root
    When user is trying to cd with a parameter, that parameter is where we want to move the cwd to
    First, we get try to get the ino of the parameter to make sure the destination exists
    If it does, verify that it is a DIR type before we can change the cwd
    Once, it passes all the checks we can then change to cwd to the specified directory.
*/
int chdir(char *pathname)
{
  /**** READ Chapter 11.7.3 HOW TO chdir ****/
  if (pathname[0] != '\0')//cd with 2nd argument
  {
    int ino = getino(pathname);// get inode # of pathname
    if (ino == 0) return 0;//doesn't exist

    MINODE *mip = iget(dev, ino);//ptr set to loaded inode with correct ino #

    // VERIFY is mip->INODE is DIR
    if (S_ISDIR(mip->INODE.i_mode))
    {   
      iput(running->cwd);
      running->cwd = mip;
    }
    else//not a DIR
    {
      printf("%s is not a DIR\n", pathname);
    }
  }
  else // When user enters cd without a second argument
  {
    running->cwd = root;
  }
}

/*** Summary ***/
/*
    In this function, we have a loop that goes through information in the data block
    Search for the ino number of wd, and also the ino number of the parent
    We use the my_ino to get the name of the path i.e. dir1/dir2....
    We use parent_ino to get a parent MINODE then recursively call rpwd(pip)
    Repeat the process to go back and back until we get all the correct directories
    Say we are in /dir1/dir2/dir3, this is where pwd is called
    It will start in dir3, get the needed information (name...)
    Recurse "back up" to dir2, get the needed informaton (name...)
    Recurse "back up" to dir1, get the needed information (name...)
    Recurse "back up" to root or "/", get the needed information (name...)
    Here it will try to recurse but fails when we can't get the parent (already at root, no parent)
    It will start to proceed with printing the "/dir1"
    Then return to which the it print "/dir2"
    Then back to the first call where it will print "/dir3"
    We used strcat to concatenate all of this into "/dir1/dir2/dir3" which is then printed to console as the wd
*/
char *retPWD[NMINODE];//used to return cwd if not root
char *rpwd(MINODE *wd)
{
  if (wd == root) return;

  for (int i = 0; i < n; i++)
  {
    if ((wd->dev != root->dev) && (wd->ino == 2))
    {
      for (int j = 0; j < NMTABLE; j++)
      {
        if (dev == mtable[j].dev)
        {
          wd = mtable[j].mptr;
          dev = wd->dev;
          break;
        }
      }
      break;
    }
  }

  int my_ino, parent_ino, x = 0;
  char *my_name[NMINODE];

  // FROM wd->INODE.i_block[0] get, my_ino & parent_ino
  char buf[BLKSIZE], temp[256];
  DIR *dp;
  char *cp;

  // Assume DIR has only one data block i_block[0]
  get_block(dev, wd->INODE.i_block[0], buf); 
  dp = (DIR *)buf;
  cp = buf;
  
  while (cp < buf + BLKSIZE && x < 2)//while still in data block
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
      parent_ino = dp->inode;//found parent_ino number
      x++;
    }
    // printf("[%d %s]  ", dp->inode, temp); // print [inode# name]

    //advancing to next data block
    cp += dp->rec_len;
    dp = (DIR *)cp;
  }

  // Clear buffers
  strcpy(buf, "");
  strcpy(temp, "");

  MINODE *pip = iget(dev, parent_ino);

  // FROM pip->INODE.i_block[ ]: get my_name string by my_ino as LOCAL
  /*
      This function loops through the data block 
      until it finds the data with the same ino number
      then save that string into the parameter "my_name"
  */
  findmyname(pip, my_ino, my_name);

  rpwd(pip);
  printf("/%s", my_name);
  strcat(retPWD, "/");
  strcat(retPWD, my_name);
}

/*** Summary ***/
/*
    In this function, we check 2 cases
    One where the current working directory is the root, and one where it is not
    When the cwd is a root, we just need to print out "/" because that is the root
    For when cwd is not the root we call rpwd(cwd)
*/
char *pwd(MINODE *wd)
{
  strcpy(retPWD, "");
  if (wd == root){ //current directory is root
    printf("CWD = /\n");
    return "/";
  }
  else //current directory not root
  {
    printf("CWD = ");
    rpwd(wd);//updates retPWD (global var)
    printf("\n");
    return retPWD;
  }
}

/*** Summary ***/
/*
    This is the ls that does all the printing of the information
    It takes a MINODE and a char * as a parameter
    The MINODE is used to print all the necessary details such as
    Type (REG | LNK | DIR)
    Print the r|w|x
    Link counts, uid, gid
    Time
    and the "-->" for LNK type files.
*/
char *t1 = "xwrxwrxwr-------";
char *t2 = "----------------";
int ls_file(MINODE *mip, char *name)
{
  char ftime[64];
  // READ Chapter 11.7.3 HOW TO ls
  if (S_ISREG(mip->INODE.i_mode)) // if (S_ISREG())
  {
    printf("%c", '-');
  }
  if (S_ISDIR(mip->INODE.i_mode)) // if (S_ISDIR())
  {
    printf("%c", 'd');
  }
  if (S_ISLNK(mip->INODE.i_mode)) // if (S_ISLNK())
  {
    printf("%c", 'l');
  }
  for (int i = 8; i >= 0; i--)
  {
    if (mip->INODE.i_mode & (1 << i)) // print r|w|x
    {
      printf("%c", t1[i]);
    }
    else if (S_ISLNK(mip->INODE.i_mode))
    {
      printf("%c", t1[i]);
    }
    else
    {
      printf("%c", t2[i]);
    }
  }

  printf("  %d  %d  %d", mip->INODE.i_links_count, mip->INODE.i_uid, mip->INODE.i_gid); // first number to show

  strcpy(ftime, ctime(&mip->INODE.i_mtime)); // print time in calendar form
  ftime[strlen(ftime) - 1] = 0;
  // kill \n at end
  printf("  %s", ftime);


  printf("  %d", mip->INODE.i_size);
  printf("  %s", name);

  if (S_ISLNK(mip->INODE.i_mode))
  {
    char linkname[1024];
    if (readlink(name, linkname, 1024) > 0)
    {
      printf(" -> %s", linkname);//print linked name
    }
  }

  printf("\n");
}

/*** Summary ***/
/*
    In this function, all we did is have a loop that runs a block of data
    Then in each section, call ls_file() on that section
    Then move the pointer accordingly
*/
int ls_dir(MINODE *mip)
{
  char buf[BLKSIZE], temp[256];
  DIR *dp;
  char *cp;

  // Assume DIR has only one data block i_block[0]
  get_block(dev, mip->INODE.i_block[0], buf); 
  dp = (DIR *)buf;
  cp = buf;
  
  while (cp < buf + BLKSIZE)
  {
    strncpy(temp, dp->name, dp->name_len);
    temp[dp->name_len] = 0;

    //printf("[%d %s]  ", dp->inode, temp); // print [inode# name]
    mip = iget(dev, dp->inode);
    ls_file(mip, temp);
    iput(mip);

    cp += dp->rec_len;
    dp = (DIR *)cp;
  }
  printf("\n");
}

/*** Summary ***/
/*
    In this function, we made it check for 2 conditions
    One where use ls without an argument, then we just ls the cwd
    The other is where the user specifies the directory they want to ls, then we ls that directory
    To ls a specified a specific directory, here are the steps:
    pwd(cwd) to save for later use
    switch to the specified directory
    ls on the current directory
    then pwd(back) to the original directory
*/
int ls(char *pathname)  
{
  printf("ls %s\n", pathname);
  if (pathname[0] != '\0')//ls specific directory
  {
    char curdir[NMINODE];
    strcpy(curdir, pwd(running->cwd));//get current directory into curdir
    printf("curdir = %s\n", curdir);
    chdir(pathname);//change to user specified directory
    ls_dir(running->cwd);//ls specified directory
    chdir(curdir);//change back to previous current directory

    // int ino = getino(pathname);
    // MINODE *mip = iget(dev, ino);
    // ls_dir(mip);
  }
  else
  {
    ls_dir(running->cwd);//ls current directory
  }
}

