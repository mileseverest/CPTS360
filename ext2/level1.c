#include "util.c"

int getino(int *dev, char *pathname)
{
    MINODE *mip = proc[0].cwd;
    char **tokenBuf;
    int inode = 0, i = 0;

    if (pathname == NULL)
    {
        inode = running->cwd->ino;    
        return inode;
    }

    if (pathname[0] == '/') // go to root
    {
        dev = root->dev;
        ip = &(root->INODE);
        inode = root->ino;
    }

    else
    {
        dev = running->cwd->dev;
        ip = &(running->cwd->INODE);
    }

    tokenBuf = tokenize(pathname, '/');
    
    while (tokenBuf[i])
    {
        inode = search(mip, pathname);

        if (inode < 1)
        {
            if (mip)
                iput(mip);

            return -1;
        }

        if (mip)
            iput(mip);

        mip = iget(dev, inode);
        ip = &(mip->INODE);

        i++;
    }

    inode = mip->ino;
    iput(mip);
}


int search(const MINODE *mip, const char *name)
{   
    char buf[BLOCK_SIZE], temp[256];
 
    int i;

    for(i = 0; i < 12; ++i)
    {
        if(mip->INODE.i_block[i] != 0)     
        {  
            get_block(mip->dev, mip->INODE.i_block[i], buf);         
            DIR *dp = (DIR *)buf;  
            char *cp = buf;
            
            while(cp < buf +  BLOCK_SIZE)
            {                                   
                strncpy(temp, dp->name, dp->name_len);                
                temp[dp->name_len] = '\0';
                                              
                if(strcmp(temp, name) == 0)
                {
                    return dp->inode;
                }
                cp += dp->rec_len;
                dp = (DIR *)cp;
            }
        }
    }
  
    return -1;
}

MINODE *iget(int dev, int ino)
{
    int i = 0, blk, offset;    
    char buf[BLOCK_SIZE];
    MINODE *mip = NULL;

    for(i = 0; i < 100; i++)        
    {                        
        if(minode[i].refCount > 0 && minode[i].ino == ino)                        
        {                                                
            mip = &minode[i];                        
            minode[i].refCount++;                        
            return mip;                        
        }                        
    }

    i = 0;
       
