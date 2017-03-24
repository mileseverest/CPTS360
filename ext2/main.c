#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include <ext2fs/ext2_fs.h>   // NOTE: Ubuntu users MAY NEED "ext2_fs.h"
#include <libgen.h>
#include <string.h>
#include <sys/stat.h>

#include "type.h"
#include "util.c"

MINODE minode[NMINODE];
MINODE *root;
PROC proc[NPROC], *running;
MOUNT mtable[NMOUNT];

char names[64][128], *name[64];
int fd, dev, n;
int nblocks, ninodes, bmap, imap, inode_start;
char line[256], cmd[32], pathname[256], readBuf[BLOCK_SIZE];

MINODE *iget(int dev, int ino);
int iput(MINODE *mip);
void ls(char pathname[]);
void cd(char *pathname);
void pwd();
void my_pwd(MINODE *mip, int cino);
int my_mkdir(char *pathname);
int k_mkdir(MINODE *mip, char *name);
int my_rmdir(char *pathname);
int rmChild(MINODE *pmip, char *name);
int findName(char *pathname);
int findino(MINODE *mip, int *ino, int *pino);

int bdealloc(int dev, int bit);
int balloc(int dev);
unsigned long idealloc(int dev, int ino);
unsigned long ialloc(int dev);
int decFreeBlocks(int dev);
int incFreeBlocks(int dev);
int decFreeInodes(int dev);
int incFreeInodes(int dev);
int set_bit(char *buf, int bit);
int clor_bit(char *buf, int bit);
int tst_bit(char *buf, int bit);

int init()
{
    int i, j;

    MINODE *mip;
    PROC *p;

    printf("In init()\n");

    for (i = 0; i < NMINODE; i++)
    {
        mip = &minode[i];
        mip->dev = mip->ino = 0;
        mip->refCount = 0;
        mip->mounted = 0;
        mip->mountptr = 0;
    }

    for (i = 0; i < NPROC; i++)
    {
        p = &proc[i];
        p->pid = i;
        p->uid = 0;
        p->cwd = 0;
        p->status = FREE;

        for (j = 0; j < NFD; j++)
            p->fd[j] = 0;
    }

    for (i = 0; i < NMOUNT; i++)
    {
        mtable[i].dev = 0;
    }
}

int mount_root()
{
    printf("In mount_root()\n");
    root = iget(dev, 2);
}

char *disk = "disk";

main(int argc, char *argv[])
{
    int ino;
    char buf[BLOCK_SIZE];
    
    if (argc < 1)
        disk = argv[1];

    printf("Checking EXT2 File System\n");

    if ((fd = open(disk, O_RDWR)) < 0)
    {
        printf("Open disk %s failed\n", disk);
        exit(1);
    }
    
    dev = fd;


    get_block(dev, 1, buf);
    sp = (SUPER *) buf;

    if (sp->s_magic != 0xEF53)
    {
        printf("Magic %x is not an EXT2 FS\n", sp->s_magic);
        exit(1);
    }

    ninodes = sp->s_inodes_count;
    nblocks = sp->s_blocks_count;

    get_block(dev, 2, buf);
    gp = (GD *) buf;

    bmap = gp->bg_block_bitmap;    
    imap = gp->bg_inode_bitmap;
    inode_start = gp->bg_inode_table;
    printf("bmp = %d imap = %d inode_start = %d\n", bmap, imap, inode_start);

    init();
    mount_root();
    
    mtable[0].dev = dev;
    mtable[0].nblocks = nblocks;
    mtable[0].ninodes = ninodes;
    mtable[0].bmap = bmap;
    mtable[0].imap = imap;
    mtable[0].mounted_inode = root;
    strncpy(mtable[0].name, "disk", strlen("disk"));

    printf("root refCount = %d\n", root->refCount);
  
    printf("creating P0 as running process\n");
    running = &proc[0];      
    running->status = READY;        
    running->cwd = iget(dev, 2);          
    printf("root refCount = %d\n", root->refCount);
    
    while(1)
    {
        printf("Input Command: [ls|cd|pwd|quit] ");
        fgets(line, 128, stdin);

        line[strlen(line) - 1] = 0;

        if (line[0] == 0)
            continue;

        pathname[0] = 0;

        sscanf(line, "%s %[^\t\n]", cmd, pathname);
        printf("Command: %s Pathname: %s\n", cmd, pathname);

        if (strcmp(cmd, "ls") == 0)
            ls(pathname);
        if (strcmp(cmd, "cd") == 0)
            cd(pathname);
        if (strcmp(cmd, "pwd") == 0)
            pwd();
        if (strcmp(cmd, "mkdir") == 0)
            my_mkdir(pathname);
        if (strcmp(cmd, "creat") == 0)
            my_creat(pathname);
        if (strcmp(cmd, "rmdir") == 0)
            my_rmdir(pathname);
        if (strcmp(cmd, "rm") == 0)
            my_rm(pathname);
        if (strcmp(cmd, "stat") == 0)
            my_stat(pathname);
        if (strcmp(cmd, "chmod") == 0)
            my_chmod(pathname);
        if (strcmp(cmd, "touch") == 0)
            my_touch(pathname);
        if (strcmp(cmd, "link") == 0)
            my_link(pathname);
        if (strcmp(cmd, "unlink") == 0)
            my_unlink(pathname);
        if (strcmp(cmd, "symlink") == 0)
            my_symlink(pathname);
        if (strcmp(cmd, "open") == 0)
            my_open(pathname, -1);
        if (strcmp(cmd, "write") == 0)
            write_file(pathname);
        if (strcmp(cmd, "read") == 0)
            read_file(pathname, -1);
        if (strcmp(cmd, "close") == 0)
            close_file(pathname);
        if (strcmp(cmd, "cat") == 0)
            my_cat(pathname);
        if (strcmp(cmd, "cp") == 0)
            my_cp(pathname);
        if (strcmp(cmd, "mv") == 0)
            my_mv(pathname);
        if (strcmp(cmd, "lseek") == 0)
            my_lseek(pathname);
        if (strcmp(cmd, "mount") == 0)
            my_mount(pathname);
        if (strcmp(cmd, "umount") == 0)
            my_umount(pathname);
        if (strcmp(line, "quit") == 0)
            quit();
    }
}

int quit()
{
    int i;
    MINODE *mip;

    for (i = 0; i < NMINODE; i++)
    {
        mip = &minode[i];
        if (mip->refCount > 0)
            iput(mip);
    }

    exit(0); 
}


int getino(int *device, char *pathname)
{
    MINODE *mip = running->cwd;//proc[0].cwd;
    INODE *cwd = malloc(sizeof(INODE));
    char **tokenBuf;
    int seek, prevIno, inode = 0, i = 0;

    if (pathname == NULL)
    {
        inode = running->cwd->ino;    
        return inode;
    }

    if (pathname[0] == '/') // go to root
    {
        mip = root;
        device = root->dev;
        ip = &(root->INODE);
        inode = root->ino;
    }

    else
    {
        device = running->cwd->dev;
        inode = running->cwd->ino;
        ip = &(running->cwd->INODE);
    }

    tokenBuf = tokenize(pathname, '/');
    prevIno = inode;
    while (tokenBuf[i])
    {
        prevIno = inode;
        inode = search(mip, tokenBuf[i]);

        if (inode < 1)
        {
            if (mip)
                iput(mip);

            return -1;
        }

        if (mip)
        {
            mip->dirty = 1; 
            iput(mip);
        }
        
        mip = iget(device, inode);
        ip = &(mip->INODE);
    
        if (mip->mounted == 1)
        {
            int j;
            for (j = 0; mtable[j].dev != mip->mountptr->dev; j++);

            switch_dev(j);
            mip = iget(mip->mountptr->dev, 2);
        }

        else if (prevIno == 2 && mip->ino == 2 && mip->dev != root->dev)
        {
            int j, k;
            for (j = 0; j < NFD && mtable[j].dev != mip->dev; j++);
            
            mip = iget(mtable[j].mounted_inode->dev, mtable[j].mounted_inode->ino);
            
            for (k = 0; mtable[k].dev != mtable[j].mounted_inode->dev; k++);
            switch_dev(k);
        }

        i++;
    }


    inode = mip->ino;

    mip->dirty = 1;
    iput(mip);
    
    return inode;
}

int search(const MINODE *mip, const char *name)
{   
    char buf[BLOCK_SIZE], temp[256];

    int j = 0;

    //for (j = 0; j < BLOCK_SIZE; j++)
       // printf("%c", buf[j]);

    int i;

    for(i = 0; i < 12; ++i)
    {
        if(mip->INODE.i_block[i] != 0)     
        { 
            get_block(mip->dev, mip->INODE.i_block[i], buf);         
            DIR *dp = (DIR *)buf;  
            char *cp = buf;
            
            while(cp < buf + BLOCK_SIZE)
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
    INODE *inodeptr;
    int j = 0;
    for(i = 0; i < 100; i++)        
    {                        
        if (minode[i].dev == dev && minode[i].ino == ino)
        {
            minode[i].refCount++;
            return &minode[i];
        }        
    }

    for(i = 0; minode[i].ino != 0 && i < 100; i++);
   
    if(i == 100)            
    {                
        printf("Error: NO SPACE IN MINODE ARRAY\n");     
        return 0;    
    }
            
    blk = (ino - 1)/8 + inode_start;
    offset = (ino - 1)%8;                    
    get_block(dev, blk, buf);                       
    //ip = (INODE *)buf + offset;                            
   
    //memcpy(&(minode[i].INODE), ip, sizeof(INODE));                               
    
    minode[i].dev = dev;                                    
    minode[i].ino = ino;                         
    memcpy(&minode[i].INODE, (((INODE *) buf) + offset), sizeof(INODE));
    minode[i].refCount = 1;
    minode[i].dirty = 0;
    minode[i].mounted = 0;
    minode[i].mountptr = NULL; 

    return &minode[i];
}

int iput(MINODE *mip)
{
    char buf[BLOCK_SIZE];    
    int blk, offset;
           
    INODE *tempip;
   
    mip->refCount--;

    if(mip->refCount > 0) 
        return 1;

    if(mip->dirty == 0) 
        return 1;

    blk = (mip->ino-1)/8 + inode_start;
    offset = (mip->ino-1)%8;
    get_block(dev, blk, buf);
    tempip = (INODE*)buf + offset;
    memcpy(tempip, &(mip->INODE), sizeof(INODE));
    put_block(mip->dev, blk, buf);
                            
    return 1;
}

void ls(char *pathname)
{ 
    int ino, i = 0; 
    MINODE *mip; 
    mip = running->cwd;
    mip->dev = running->cwd->dev;
    char **path, *token;
    
    if (strcmp(pathname, "/") == 0)
    {
        printdir(root);
        return;
    }

    if(pathname[0] == '/') 
    {
        path = tokenize(pathname, '/');
        mip = root;
        mip->dev = root->dev;
        while (path[i] != NULL)
        { 
            ino = search(mip, path[i]);
            printf("ino: %d\n", ino); 
            if(ino == -1)                                       
            {                       
                printf("directory does not exist\n");             
                iput(mip);
                return;
            }
                
            mip = iget(dev,ino);
            i++;
        }
    }
    
    printdir(mip);
}

void printdir(MINODE *mip)
{ 
    char buf[1024];

    get_block(dev,mip->INODE.i_block[0], buf);
    int i;
    DIR *dp = (DIR*)buf;
    char *cp = buf;
    
/*    while(cp < buf + BLOCK_SIZE)
    {
        char temp[100];
        strncpy(temp, dp->name, dp->name_len);
        temp[dp->name_len] = 0;
        printf("%s   ", temp);
        cp += dp->rec_len;
        dp = (DIR *)cp;
        memset(temp, 0, 100);
    }
*/
    for(i = 0; i < 12; ++i)       
    {
        if(mip->INODE.i_block[i] == 0)
        {
            return 0;
        }

        char buffer[BLOCK_SIZE];
        get_block(dev, mip->INODE.i_block[i], buffer);
        DIR *dir = (DIR *)buffer;
        int pos = 0;

        while(pos < BLOCK_SIZE)          
        {
            char dirname[dir->name_len + 1];
            strncpy(dirname, dir->name, dir->name_len);
            dirname[dir->name_len] = '\0';
            MINODE *curmip = iget(mip->dev, dir->inode);

            printf( (curmip->INODE.i_mode & 0x4000) ? "D" : "");
            printf( (curmip->INODE.i_mode & 0x8000) == 0x8000 ? "R" : "");
            printf( (curmip->INODE.i_mode & 0xA000) == 0xA000 ? "S" : "");
        
            char *badctime = ctime(&curmip->INODE.i_mtime);            
            badctime[24] = '\0';

            printf( (curmip->INODE.i_mode & 0x0100) ? " r" : " -");
            printf( (curmip->INODE.i_mode & 0x0080) ? "w" : "-");
            printf( (curmip->INODE.i_mode & 0x0040) ? "x" : "-");
            printf( (curmip->INODE.i_mode & 0x0020) ? "r" : "-");
            printf( (curmip->INODE.i_mode & 0x0010) ? "w" : "-");
            printf( (curmip->INODE.i_mode & 0x0008) ? "x" : "-");
            printf( (curmip->INODE.i_mode & 0x0004) ? "r" : "-");
            printf( (curmip->INODE.i_mode & 0x0002) ? "w" : "-");
            printf( (curmip->INODE.i_mode & 0x0001) ? "x" : "-");
        
            printf("\t%d\t%d\t%d\t%s\t%s", curmip->INODE.i_uid, curmip->INODE.i_gid, curmip->INODE.i_size, badctime, dirname);
            if (S_ISLNK(curmip->INODE.i_mode))
                printf(" -> %s\n", (char *) curmip->INODE.i_block);
            
            else
                printf("\n");

            iput(curmip);
            char *loc = (char *)dir;
            loc += dir->rec_len;
            pos += dir->rec_len;
            dir = (DIR *)loc;
        }
    }

    return 0;
}

void cd(char *pathname)
{
    MINODE *mip;
    char **path; 
    if (!pathname[0])
    {
        running->cwd = iget(root->dev, 2);
        return 0;
    }

    if (strcmp(pathname, "/") == 0)
    {
        running->cwd = iget(root->dev, 2);
        return 0;
    }

    int ino = getino(dev, pathname);

    if (ino == -1)
    {
        printf("Directory does not exist!\n");
        return 0;
    }
    
    mip = iget(dev, ino);

    if ((mip->INODE.i_mode & 0100000) == 0100000)
    {
        iput(mip);
        printf("Not a directory!\n");
        return -1;
    }

    running->cwd = mip;
    
    return 0;
}

void pwd()
{
    my_pwd(running->cwd, 0);
    printf("\n");
    return;
}

void my_pwd(MINODE *mip, int cino)
{
    if (mip->ino == root->ino)
        printf("/");

    char buf[BLOCK_SIZE], *cp, name[64];
    DIR *dp;
    MINODE *pip;

    get_block(fd, mip->INODE.i_block[0], buf);
    dp = (DIR *) buf;
    cp = buf + dp->rec_len;
    dp = (DIR *) cp;
    if (mip->ino != root->ino)
    {
        int ino = dp->inode;
        pip = iget(fd, ino);
        my_pwd(pip, mip->ino);
    }

    if (cino != 0)
    {
        while (dp->inode != cino)
        {
            cp += dp->rec_len;
            dp = (DIR *) cp;
        }

        strncpy(name,dp->name,dp->name_len);
        name[dp->name_len] = '\0';   
        printf("%s/",name);
    }

    return;
}

int findpathname(MINODE *mip, int ino, char *name)
{
    int i;
    char buf[BLOCK_SIZE], nameBuf[256], *cp;
    DIR *dp;

    for (i = 0; i < 12; ++i)
    {
        if (mip->INODE.i_block[i] != 0)
        {
            get_block(mip->dev, mip->INODE.i_block[i], buf);
        
            dp = (DIR *) buf;
            cp = buf;

            while (cp < &buf[BLOCK_SIZE])
            {
                strncpy(nameBuf, dp->name, dp->name_len);                
                nameBuf[dp->name_len] = 0;

                if (dp->inode = ino)
                {
                    strcpy(name, nameBuf);
                    return 1;
                }

                cp += dp->rec_len;
                dp = (DIR *) cp;
            }
        }
    }

    return -1;
}

int my_mkdir(char *pathname)
{
    int ino, pino, i;
    MINODE *mip, *pip;
    char *parent, *child;
    char *temp;
    if (pathname[0] == '/')
        dev = root->dev;

    else
        dev = running->cwd->dev;

    if (findName(pathname))
    {
        parent = dirname(pathname);
        child = basename(pathname);
        temp = (char *) malloc((strlen(parent) + 1) * sizeof(char));
        strcpy(temp, parent);
        pino = getino(&dev, temp);

        if (pino == -1)
            return -1;

        pip = iget(dev, pino);
    }
    
    else
    {
        pip = iget(running->cwd->dev, running->cwd->ino);
        child = (char *) malloc((strlen(pathname) + 1) * sizeof(char));
        strcpy(child, pathname);
    }

    printf("Parent Child: %s %s\n", parent, child);

    if ((pip->INODE.i_mode & 0x4000) != 0x4000)
    {
        printf("Not a directory!\n");
        return -1;
    }

    if (search(pip, child) != -1)
    {
        printf("Directory already exists!\n");
        return -1;
    }

    k_mkdir(pip, child);
}

int k_mkdir(MINODE *pip, char *name)
{
    int i;
    int ino = ialloc(dev), blk = balloc(dev);
    if (ino == 0)
    {
        printf("Unable to allocate inode\n");
        return -1;
    }
    dev = pip->dev;

    MINODE *mip = iget(dev, ino);

    mip->INODE.i_mode = 0x41ED;
    mip->INODE.i_uid = running->uid;
    mip->INODE.i_gid = running->gid;
    mip->INODE.i_size = BLOCK_SIZE;
    mip->INODE.i_links_count = 2;
    mip->INODE.i_blocks = BLOCK_SIZE / 512;
    mip->INODE.i_atime = time(0L);
    mip->INODE.i_mtime = time(0L);
    mip->INODE.i_ctime = time(0L);

    for (i = 0; i < 12; i++)
        mip->INODE.i_block[i] = 0;
    
    mip->ino = ino; 
    mip->INODE.i_block[0] = blk;
    mip->dirty = 1;
    iput(mip);
    
    DIR *dp;
    char *cp, buf[BLOCK_SIZE];

    memset(buf, 0, BLOCK_SIZE);

    //get_block(dev, blk, buf);

    dp = (DIR *) buf;
    cp = buf;
    
    int length = (4 *((8 + strlen(".") + 3) / 4));

    dp->inode = ino;
    dp->name_len = strlen(".");
    dp->rec_len = length; //(4 * ((8 + dp->name_len + 3) / 4));
    dp->file_type = EXT2_FT_DIR;
    strncpy(dp->name, ".", dp->name_len);

    cp = buf;
    cp += dp->rec_len; 
    dp = (DIR *) cp;
    
    printf("Parent ino: %d\n", pip->ino);

    dp->inode = pip->ino;
    dp->name_len = strlen("..");
    dp->rec_len = BLOCK_SIZE - length;
    strncpy(dp->name, "..", dp->name_len);

    printf("test\n");
    put_block(dev, blk, buf);
    
    //lseek(dev, pip->INODE.i_block[0]*BLOCK_SIZE, SEEK_SET);
    //read(dev, buf, BLOCK_SIZE);

    get_block(dev, pip->INODE.i_block[0], buf);

    dp = (DIR *) buf;
    cp = buf;

    int rec_len = 0;

    while (cp + dp->rec_len < buf + BLOCK_SIZE)
    {
        cp += dp->rec_len;
        dp = (DIR *) cp;
    }

    int need_len = 4 * ((8 + strlen(name) + 3) / 4);
    int ideal_len = 4 * ((8 + dp->name_len + 3) / 4);

    int remain = dp->rec_len - ideal_len;

    if (remain >= ideal_len)
    {
        dp->rec_len = ideal_len;
        cp+=dp->rec_len;
        dp = (DIR *)cp;
        dp->rec_len = remain;
        dp->name_len = strlen(name);
        strncpy(dp->name, name, dp->name_len);
        dp->inode = ino;
    
        put_block(dev, pip->INODE.i_block[0], buf);
    }

    else
    {
        i = 0;

        while (remain < ideal_len)
        {
            i++;

            if (pip->INODE.i_block[i] == 0)
            {
                pip->INODE.i_block[i] = balloc(dev);
                pip->refCount = 0;
                remain = BLOCK_SIZE;
                memset(buf, 0, BLOCK_SIZE);
                cp = buf;
                dp = (DIR *) buf;
            }

            else
            {
                get_block(dev, pip->INODE.i_block[i], buf);
                cp = buf;
                dp = (DIR *) buf;

                while (cp + dp->rec_len < buf + BLOCK_SIZE)
                {
                    cp += dp->rec_len;
                    dp = (DIR *) cp;
                }

                need_len = 4 * ((8 + dp->name_len + 3) / 4);

                remain = dp->rec_len - need_len;

                if (remain >= need_len)
                {
                    dp->rec_len = need_len;
                    cp += dp->rec_len;
                    dp = (DIR *) cp;
                }
            }
        }

        dp->rec_len = remain;
        dp->name_len = strlen(name);
        dp->inode = mip->ino;
        strncpy(dp->name, name, strlen(name));

        put_block(dev, pip->INODE.i_block[i], buf);
    }

    pip->dirty = 1;
    pip->refCount++;
    pip->INODE.i_atime = time(0);
    iput(pip);
    return;
//    enter_child(pip, ino, name, 1);

    //return 1;
}

int enter_child(MINODE *pip, int ino, char *name, int mode)
{
    int i, blk;
    DIR *dp;
    char *cp, buf[BLOCK_SIZE];

    dev = pip->dev;

    for (i = 0; pip->INODE.i_block[i]; i++);
    
    i--;

    get_block(dev, pip->INODE.i_block[i], buf);
    
    dp = (DIR *) buf;
    cp = buf;

    int rec_len = 0;

    while (cp + dp->rec_len < buf + BLOCK_SIZE)
    {
        cp += dp->rec_len;
        dp = (DIR *) cp;
    }

    int need_len = 4 * ((8 + strlen(name) + 3) / 4);
    int ideal_len = 4 * ((8 + dp->name_len + 3) / 4);

    int remain = dp->rec_len - ideal_len;

    if (remain >= ideal_len)
    {
        dp->rec_len = ideal_len;
        cp+=dp->rec_len;
        dp = (DIR *)cp;
        dp->rec_len = remain;
        dp->name_len = strlen(name);
        strncpy(dp->name, name, dp->name_len);
        dp->inode = ino;
    
        put_block(dev, pip->INODE.i_block[i], buf);
    }

    else
    {
        i = 0;

        while (remain < ideal_len)
        {
            i++;

            if (pip->INODE.i_block[i] == 0)
            {
                pip->INODE.i_block[i] = balloc(dev);
                pip->refCount = 0;
                remain = BLOCK_SIZE;
                memset(buf, 0, BLOCK_SIZE);
                cp = buf;
                dp = (DIR *) buf;
            }

            else
            {
                get_block(dev, pip->INODE.i_block[i], buf);
                cp = buf;
                dp = (DIR *) buf;

                while (cp + dp->rec_len < buf + BLOCK_SIZE)
                {
                    cp += dp->rec_len;
                    dp = (DIR *) cp;
                }

                need_len = 4 * ((8 + dp->name_len + 3) / 4);

                remain = dp->rec_len - need_len;

                if (remain >= need_len)
                {
                    dp->rec_len = need_len;
                    cp += dp->rec_len;
                    dp = (DIR *) cp;
                }
            }
        }

        dp->rec_len = remain;
        dp->name_len = strlen(name);
    //    dp->inode = mip->ino;
        strncpy(dp->name, name, strlen(name));

        put_block(dev, pip->INODE.i_block[i], buf);
    }

    pip->dirty = 1;
    pip->refCount++;
    pip->INODE.i_atime = time(0);
    iput(pip);
    return;

  /*      int blk = balloc(dev);
        pip->INODE.i_block[i] = blk;
        pip->INODE.i_size += BLOCK_SIZE;

        memset(buf, 0, BLOCK_SIZE);
        get_block(dev, pip->INODE.i_block[i], buf);
        
        dp = (DIR *) buf;
        dp->rec_len = BLOCK_SIZE;
        dp->name_len = strlen(name);
        strncpy(dp->name, name, dp->name_len);
        dp->inode = ino;
        
        put_block(dev, pip->INODE.i_block[i], buf);
    }

    if (mode == 1)
       pip->INODE.i_links_count++;    
    
    pip->INODE.i_atime = time(0L);

    pip->dirty = 1;
    iput(pip);

    return 1;*/
}

int my_creat(char *pathname)
{
    int ino, pino, i;
    MINODE *mip, *pip;
    char *parent, *child;

    if (pathname[0] == '/')
        dev = root->dev;

    else
        dev = running->cwd->dev;

    if (findName(pathname))
    {
        parent = dirname(pathname);
        child = basename(pathname);
        pino = getino(&dev, parent);

        if (pino == -1)
            return -1;

        pip = iget(dev, pino);
    }
    
    else
    {
        pip = iget(running->cwd->dev, running->cwd->ino);
        child = (char *) malloc((strlen(pathname) + 1) * sizeof(char));
        strcpy(child, pathname);
    }

    if ((pip->INODE.i_mode & 0x4000) != 0x4000)
    {
        printf("Not a directory!\n");
        return -1;
    }

    if (search(pip, child) != -1)
    {
        printf("File already exists!\n");
        return -1;
    }

    k_creat(pip, child);
}

int k_creat(MINODE *pip, char *name)
{
    int i;
    int ino = ialloc(dev);
    
    if (ino == 0)
    {
        printf("Unable to allocate inode\n");
        return -1;
    }
    dev = pip->dev;

    MINODE *mip = iget(dev, ino);

    mip->INODE.i_mode = 0x81A4;
    mip->INODE.i_uid = running->uid;
    mip->INODE.i_gid = running->gid;
    mip->INODE.i_size = 0;
    mip->INODE.i_links_count = 1;
    mip->INODE.i_blocks = BLOCK_SIZE / 512;
    mip->INODE.i_atime = time(0L);
    mip->INODE.i_mtime = time(0L);
    mip->INODE.i_ctime = time(0L);

    for (i = 0; i < 12; i++)
        mip->INODE.i_block[i] = 0;
    
    mip->dirty = 1;
    iput(mip);

    enter_child(pip, ino, name, 0);

    return 1;
}

int my_rmdir(char *pathname)
{
    int ino, i, entries = 0, pino;
    MINODE *mip, *pip;
    char buf[BLOCK_SIZE], *myName, *cp, name[64], parentDir[64];
    char *parent, *child, *temp;
    if (!pathname[0])
    {
        printf("Please enter file name!\n");
        return -1;
    }

    if (findName(pathname))
    {
        parent = dirname(pathname);
        child = basename(pathname);
        temp = (char *) malloc((strlen(parent) + 1) * sizeof(char));
        strcpy(temp, parent);
        pino = getino(&dev, temp);

        if (pino == -1)
            return -1;

        pip = iget(dev, pino);
    }
    
    else
    {
        pip = iget(running->cwd->dev, running->cwd->ino);
        child = (char *) malloc((strlen(pathname) + 1) * sizeof(char));
        strcpy(child, pathname);
    }
    /*cp = strrchr(pathname, '/');

    if (cp == NULL)
    {
        pino = running->cwd->ino;
        strcpy(name, pathname);
    }

    else
    {
        *(cp) = '\0';
        strcpy(parentDir, pathname);
        pino = getino(dev, parentDir);
        strcpy(name, cp + 1);
    }*/

    ino = getino(dev, child);
    mip = iget(dev, ino);

    //pip = iget(dev, pino);

    if (ino == -1 || pino == -1)
    {
        printf("File does not exist!\n");
        return -1;
    }

    if (!S_ISDIR(mip->INODE.i_mode))
    {
        printf("File not a directory!\n");
        iput(mip);
        return -1;
    }
    if (mip->refCount != 1)
    {
        iput(mip); 
        return -1;
    }

    for (i = 0; i < 12; ++i)
    {
        if (mip->INODE.i_block[i] != 0)
        {
            get_block(mip->dev, mip->INODE.i_block[i], buf);
            DIR *dp = (DIR *) buf;
            char *cp = buf;
            
            while (cp < buf + BLOCK_SIZE)
            {
                entries++;

                cp += dp->rec_len;
                dp = (DIR *) cp;
            }
        }
    }
    
    if (entries > 2)
    {
        printf("Can't remove non-empty directory!\n");
        iput(mip);
        return -1;
    }

    printf("Parent ino: %d %d\n", pino, ino); 
    
    truncate(mip);
    
    //bdealloc(dev, mip->INODE.i_block[0]);
    
    iput(mip);
    idealloc(dev, mip->ino);

    rmChild(pip, child);

    return 0;
}

int truncate(MINODE *mip)
{
    deallocInodeBlk(mip);
    mip->INODE.i_atime = time(0L);
    mip->INODE.i_mtime = time(0L);
    mip->INODE.i_size = 0;
    mip->dirty = 1;
}

int deallocInodeBlk(MINODE *mip)
{
    char bitmap[BLOCK_SIZE], buf[BLOCK_SIZE], dbuf[BLOCK_SIZE];
    int i, j, iblk, dblk;
    int indirect, doubleIndirect;

    get_block(dev, BBITMAP, bitmap);

    for (i = 0; i < 12; i++)
    {
        if (mip->INODE.i_block[i] != 0)
        {
            clr_bit(bitmap, mip->INODE.i_block[i]);
            mip->INODE.i_block[i] = 0;
        }

        else
        {
            put_block(dev, BBITMAP, bitmap);
            return;
        }
    }

    if (mip->INODE.i_block[i] != 0)
    {
        iblk = mip->INODE.i_block[i];
        get_block(dev, iblk, buf);
        indirect = (int *) buf;
        
        for (i = 0; i < 256; i++)
        {
            if (indirect != 0)
            {
                clr_bit(bitmap, indirect - 1);
                indirect = 0;
                indirect++;
            }

            else
            {
                clr_bit(bitmap, iblk - 1);
                put_block(dev, iblk, buf);
                put_block(dev, BBITMAP, bitmap);
                mip->INODE.i_block[12] = 0;
                return;
            }
        }
    }

    else
    {
        put_block(dev, BBITMAP, bitmap);
        return;
    }

    if (mip->INODE.i_block[13] != 0)
    {
        dblk = mip->INODE.i_block[13];
        get_block(dev, dblk, dbuf);

        doubleIndirect = (int *) dbuf;

        for (i = 0; i < 256; i++)
        {
            iblk = doubleIndirect;
            get_block(dev, iblk, buf);
            indirect = (int *) buf;

            for (j = 0; j < 256; j++)
            {
                if (indirect != 0)
                {
                    clr_bit(bitmap, indirect - 1);
                    indirect = 0;
                    indirect++;
                }

                else
                {
                    clr_bit(bitmap, iblk - 1);
                    clr_bit(bitmap, dblk - 1);
                    put_block(dev, iblk, buf);
                    put_block(dev, BBITMAP, bitmap);
                    put_block(dev, dblk, dbuf);
                    mip->INODE.i_block[13] = 0;
                    return;
                }

                clr_bit(bitmap, iblk - 1);
            }

            doubleIndirect++;

            if (doubleIndirect == 0)
            {
                clr_bit(bitmap, iblk - 1);
                clr_bit(bitmap, dbuf - 1);
                put_block(dev, iblk, buf);
                put_block(dev, BBITMAP, bitmap);
                put_block(dev, dblk, dbuf);
                mip->INODE.i_block[13] = 0;
                return;
            }
        }
    }

    else
    {
        put_block(dev, BBITMAP, bitmap);
        return;
    }
}

int findino(MINODE *mip, int *ino, int *pino)
{ 
    int i;
    char buf[BLOCK_SIZE], *cp;

    get_block(mip->dev, mip->INODE.i_block[0], buf);
    dp = (DIR *)buf;
    cp = buf;
                             
    *ino = dp->inode;
    cp +=dp->rec_len;
    dp = (DIR *)cp;
    *pino = dp->inode;         

    return 0;
}

int rmChild(MINODE *pip, char *name)
{
    char buf[BLOCK_SIZE];

    get_block(dev, pip->INODE.i_block[0], buf);

    char *cp = buf;
    DIR *dp = (DIR *) buf;
    int temp, i = 0, flag, last = 0;
    
    char *lastcp = buf;

    while (lastcp + dp->rec_len < buf + BLOCK_SIZE)
    {
        lastcp += dp->rec_len;
        dp = (DIR *) lastcp;
    }

    dp = (DIR *) cp;

    while (cp < buf + BLOCK_SIZE)
    {
        if (dp->name_len == strlen(name))
        {
            if (strncmp(name, dp->name, dp->name_len) == 0)
            {
                temp = dp->rec_len;

                if (cp == lastcp)
                {
                    dp = (DIR *) last;
                    dp->rec_len += temp;
                    break;
                }

                else
                {
                    dp = (DIR *) lastcp;
                    dp->rec_len += temp;
                    memcpy(cp, cp+temp, BLOCK_SIZE - i - temp);
                }

                break;
            }
        }

        last = (int) cp;
        i += dp->rec_len;
        cp += dp->rec_len;
        dp = (DIR *) cp;
    }
    
    put_block(dev, pip->INODE.i_block[0], buf);

    return 0;
}  

int findName(char *pathname)
{
    int i;

    for (i = 0; i < strlen(pathname); i++)
    {
        if (pathname[i] == '/')
            return 1;
    }

    return 0;
}

int tst_bit(char *buf, int bit)
{
  int i, j;

  i = bit / 8;
  j = bit % 8;
  if (buf[i] & (1 << j)){
    return 1;
  }
  return 0;
}

int clr_bit(char *buf, int bit)
{
  int i, j;
  i = bit / 8;
  j = bit % 8;
  buf[i] &= ~(1 << j);
  return 0;
}

int set_bit(char *buf, int bit)
{
  int i, j;
  i = bit / 8;
  j = bit % 8;
  buf[i] |= (1 << j);
  return 0;
}

int incFreeInodes(int dev)
{
  char buf[BLOCK_SIZE];

  // inc free inodes count in SUPER and GD
  get_block(dev, 1, buf);
  sp = (SUPER *)buf;
  sp->s_free_inodes_count++;
  put_block(dev, 1, buf);

  get_block(dev, 2, buf);
  gp = (GD *)buf;
  gp->bg_free_inodes_count++;
  put_block(dev, 2, buf);
}

int decFreeInodes(int dev)
{
  	char buf[1024];
	char buf2[1024];
	get_block(dev, 1, buf);
	sp = (SUPER *)buf;
	sp->s_free_inodes_count--;
	put_block(dev, 1, buf);
	get_block(dev, 2, buf2);
	gp = (GD *)buf2;
	gp->bg_free_inodes_count--;
	put_block(dev, 2, buf2);
}

int incFreeBlocks(int dev)
{
  char buf[BLOCK_SIZE];

  // inc free block count in SUPER and GD
  get_block(dev, 1, buf);
  sp = (SUPER *)buf;
  sp->s_free_blocks_count++;
  put_block(dev, 1, buf);

  get_block(dev, 2, buf);
  gp = (GD *)buf;
  gp->bg_free_blocks_count++;
  put_block(dev, 2, buf);
}

int decFreeBlocks(int dev)
{
	char buf[1024];
	char buf2[1024];
	get_block(dev, 1, buf);
	sp = (SUPER *)buf;
	sp->s_free_blocks_count--;
	put_block(dev, 1, buf);
	get_block(dev, 2, buf2);
	gp = (GD *)buf2;
	gp->bg_free_blocks_count--;
	put_block(dev, 2, buf2);
}


unsigned long ialloc(int dev)
{
 int i;
 char buf[BLOCK_SIZE];

 // get inode Bitmap into buf
 get_block(dev, imap, buf);
 
 for (i=0; i < ninodes; i++){
   if (tst_bit(buf, i)==0){
     set_bit(buf, i);
     put_block(dev, imap, buf);

     // update free inode count in SUPER and GD
     decFreeInodes(dev);
     
     printf("ialloc: ino=%d\n", i+1);
     return (i+1);
   }
 }
 return 0;
} 

unsigned long idealloc(int dev, int ino)
{
  int i;  
  char buf[BLOCK_SIZE];

  if (ino > ninodes){
    printf("inumber %d out of range\n", ino);
    return;
  }

  // get inode bitmap block
  get_block(dev, bmap, buf);
  clr_bit(buf, ino-1);

  // write buf back
  put_block(dev, imap, buf);

  // update free inode count in SUPER and GD
  incFreeInodes(dev);
}

int balloc(int dev)
{ 
    int i = 0, inodeCount = 0;
    char buf[1024];

    inodeCount = 1440;//sp->s_blocks_count; // /   (BLOCK_SIZE*128);

    get_block(dev, (gp->bg_block_bitmap),(char*)&buf );

    for(i = 0; i < inodeCount; i++)
    {
        if (tst_bit((char*)&buf, i) == 0)
        {
            set_bit((char*)&buf, i);
            put_block(dev,gp->bg_block_bitmap,(char*)&buf);
            decFreeBlocks(dev);
            return i + 1;
        }
    }

    return -1;
}    


int bdealloc(int dev, int bit)
{
    int i;   
    char buf[BLOCK_SIZE];

    get_block(dev, BBITMAP, buf);
    clr_bit(buf,bit-1);
    put_block(dev, BBITMAP,buf);

    incFreeBlocks(dev);
}

int my_stat(char *pathname)
{
    struct stat mystat;

    if (!pathname[0])
    {
        printf("No file specified!\n");
        return -1;
    }

    stat_helper(pathname, &mystat);
}

int stat_helper(char *pathname, struct stat *stp)
{
    int ino;
    MINODE *mip;
    dev = running->cwd->dev;

    if (!pathname[0])
        ino = running->cwd->ino;
        
    else
    {
        if (pathname[0] == '/')
            dev = root->dev;

        ino = getino(dev, pathname);
    }

    if (!ino)
        return -1;

    mip = iget(dev, ino);

    stp->st_dev = dev;
    stp->st_ino = ino;
    stp->st_mode = mip->INODE.i_mode;
    stp->st_uid = mip->INODE.i_uid;
    stp->st_size = mip->INODE.i_size;
    stp->st_blksize = BLOCK_SIZE;
    stp->st_atime = mip->INODE.i_atime;
    stp->st_ctime = mip->INODE.i_ctime;
    stp->st_mtime = mip->INODE.i_mtime;
    stp->st_gid = mip->INODE.i_gid;
    stp->st_nlink = mip->INODE.i_links_count;
    stp->st_blocks = mip->INODE.i_blocks;

    printf("File: %s\n", pathname);
    printf("Size: %d\t", stp->st_size);
    printf("Blocks: %d\t", stp->st_blocks / 2);
    printf("IO Block: %d\t", BLOCK_SIZE);
    printf((stp->st_mode & 0x4000) ? "Dir " : "");
    printf((stp->st_mode & 0x8000) == 0x8000 ? "Reg " : "");  
    printf((stp->st_mode & 0xA000) == 0xA000 ? "Sym " : "");
    printf("\n");

    printf("Device: %d\t", stp->st_dev);
    printf("Inode: %d\t", stp->st_ino);
    printf("Links: %d\n", stp->st_nlink);
    
    printf("Access: ");
    printf((stp->st_mode & 0x0100) ? "r" : "-");
    printf((stp->st_mode & 0x0080) ? "w" : "-");
    printf((stp->st_mode & 0x0040) ? "x" : "-");
    printf((stp->st_mode & 0x0020) ? "r" : "-");
    printf((stp->st_mode & 0x0010) ? "w" : "-");
    printf((stp->st_mode & 0x0008) ? "x" : "-");
    printf((stp->st_mode & 0x0004) ? "r" : "-");
    printf((stp->st_mode & 0x0002) ? "w" : "-");
    printf((stp->st_mode & 0x0001) ? "x" : "-");
    printf("\t");
    printf("Uid: %d\t", stp->st_uid);
    printf("Gid: %d\n", stp->st_gid);

    printf("Access: %s", ctime(&(stp->st_atime)));
    printf("Modify: %s", ctime(&(stp->st_mtime)));
    printf("Change: %s", ctime(&(stp->st_ctime)));

    iput(mip);

    return 0;
}

int my_chmod(char *pathname)
{
    int i, ino;
    int mode;
    MINODE *mip;
    
    char *mod = strtok(pathname, " ");

    if (!mod[0])
    {
        printf("Please specify both mode and filename\n");
        return -1;
    }
    
    char *name = strtok(NULL, " ");

    if (!name[0])
    {
        printf("Please specify both mode and filename\n");
        return -1;
    }
    
    dev = running->cwd->dev;
    sscanf(mod, "%x", &mode); 
    ino = getino(&dev, name);
    
    if (ino == -1)
    {
        printf("File does not exist!\n");
        return -1;
    }

    mip = iget(dev, ino);

    mode = mod[0] - 48 << 6;
    mode |= mod[1] - 48 << 3;
    mode |= mod[2] - 48;
    mip->INODE.i_mode &= 0xFF000;
    mip->INODE.i_mode |= mode;

    mip->dirty = 1;
    mip->INODE.i_atime = mip->INODE.i_mtime = time(0);
    iput(mip);

    return 0;
}

int my_rm(char *pathname)
{
    int ino, pino;
    MINODE *mip, *pip;
    char *parent, *child;
    if (findName(pathname))
    {
        parent = dirname(pathname);
        child = basename(pathname);
        pino = getino(&dev, parent);

        if (pino == -1)
            return -1;

        pip = iget(dev, pino);
    }
    
    else
    {
        pip = iget(running->cwd->dev, running->cwd->ino);
        child = (char *) malloc((strlen(pathname) + 1) * sizeof(char));
        strcpy(child, pathname);
    }

    ino = getino(dev, child);
    mip = iget(dev, ino);

    if (!S_ISREG(mip->INODE.i_mode))
    {
        printf("Can't remove non regular file!\n");
        iput(pip);
        return -1;
    }

    truncate(mip);
    iput(mip);
    idealloc(dev, mip->ino);

    rmChild(pip, child);

    return 0;
}

int my_touch(char *pathname)
{
    int ino;
    MINODE *mip;

    if (!pathname[0])
    {
        printf("No file name specified!\n");
        return -1;
    }

    dev = running->cwd->dev;

    ino = getino(&dev, pathname);
    
    if (ino > 0)
    {
        mip = iget(dev, ino);
        mip->INODE.i_atime = mip->INODE.i_mtime = time(0L);

        mip->dirty = 1;

        iput(mip);
    }

    else
    {
        my_creat(pathname);
    }

    return 0;
}
int my_link(char *pathname)
{
    char oldName[128], newName[128];
    sscanf(pathname, "%s %s", oldName, newName);

 //   oldName = strtok(pathname, " ");
 //   newName = strtok(NULL, " ");
    printf("link: %s %s\n", oldName, newName);

    if (!oldName[0] || !newName[0])
    {
        printf("Please specify two file names!\n");
        return -1;
    }
    
    char pDir[64], name[64], *cp, buf[BLOCK_SIZE];
    DIR *dp;
    MINODE *pip, *tip;
    int pino, i;

    cp = strrchr(newName, '/');

    if (cp==NULL)
    {
        pino = running->cwd->ino;
        strcpy(name, newName);
    }

    else
    {
        *(cp) = '\0';
        strcpy(pDir, newName);
        printf("parent dir: %s\n", pDir);
        pino = getino(dev, pDir);
        strcpy(name, cp + 1);
    }

    int tino = getino(dev, oldName);
    
    if (tino < 1 || pino < 1)
    {
        printf("%d %d\n", tino, pino);
        printf("One of the files does not exist!\n");
        return -1;
    }

    pip = iget(dev, pino);
    printf("name: %s\n", name); 
    if (search(&(pip->INODE), name) != -1)
    {
        printf("name: %s\n", name);
        printf("File already exists!\n");
        return -1;
    }

    tip = iget(dev, tino);

    if((tip->INODE.i_mode & 0100000) != 0100000)
    {
        iput(pip);
        printf("Cannot link to a directory!\n");
        return -1;
    }

    get_block(dev, pip->INODE.i_block[0], buf);

    cp = buf;
    dp = (DIR *) buf;

    while(cp + dp->rec_len < buf + BLOCK_SIZE)
    {
        cp += dp->rec_len;
        dp = (DIR *) cp;
    }

    int need_length = 4*((8+dp->name_len+3)/4);

    int temp = dp->rec_len - need_length;

    if (temp >= need_length)
    {
        dp->rec_len = need_length;

        cp += dp->rec_len;    
        dp = (DIR *)cp;

        dp->rec_len = temp;
        dp->name_len = strlen(name);
        dp->inode =tip->ino;
        strncpy(dp->name, name, strlen(name));

        put_block(dev, pip->INODE.i_block[0], buf);
    }

    else // need new block
    {
        i = 0;

        while (temp < need_length)
        {
            i++;

            if (pip->INODE.i_block[i] == 0)
            {
                pip->INODE.i_block[i] = balloc(dev);
                pip->refCount = 0;
                temp = BLOCK_SIZE;
                memset(buf, 0, BLOCK_SIZE);
                cp = buf;
                dp = (DIR *) buf;
            }

            else
            {
                get_block(dev, pip->INODE.i_block[i], buf);
                cp = buf;
                dp = (DIR *) buf;

                while (cp + dp->rec_len < buf + BLOCK_SIZE)
                {
                    cp += dp->rec_len;
                    dp = (DIR *) cp;
                }

                need_length = 4*((8+dp->name_len+3)/4);
                temp = dp->rec_len - need_length;

                if (temp >= need_length)
                {
                    dp->rec_len = need_length;
                    cp += dp->rec_len;             
                    dp = (DIR *)cp;
                }
            }
        }

        dp->rec_len = temp;
        dp->name_len = strlen(name);
        dp->inode = tip->ino;

        strncpy(dp->name, name, dp->name_len);

        put_block(dev, pip->INODE.i_block[i], buf);
    }

    pip->dirty = 1;
    pip->refCount++;
    pip->INODE.i_atime = time(0);
    iput(pip);
    //tip->dirty = 1;
    tip->INODE.i_links_count++;
    tip->dirty = 1;
    iput(tip);
    return tip->ino;
}

int my_symlink(char *pathname)
{
    char *oldName, *newName;

    oldName = strtok(pathname, " ");
    newName = strtok(NULL, " ");

    if (!oldName[0] || !newName[0])
    {
        printf("Please specify two file names!\n");
        return -1;
    }

    if (pathname[0] == '/')
        dev = root->dev;

    else
        dev = running->cwd->dev;

    int i, ino, pino, created;
    MINODE *mip, *pip;
    char *cp, *parent, *child, buf[BLOCK_SIZE];

    ino = getino(&dev, oldName);

    if (ino == -1)
        return -1;
    
    mip = iget(dev, ino);

    if(((mip->INODE.i_mode) & 0100000) != 0100000 && (((mip->INODE.i_mode) & 0040000) != 0040000))
    {
        printf("File is not a directory or regular file!\n");
        iput(mip);
        return -1;
    }

    iput(mip);

    if (findName(newName))
    {
        parent = dirname(newName);
        child = basename(newName);
        pino = getino(&dev, parent);

        if (pino == -1)
            return -1;

        pip = iget(dev, pino);
    }

    else
    {
        pip = iget(running->cwd->dev, running->cwd->ino);
        child = (char *)malloc((strlen(newName) + 1) * sizeof(char));
        strcpy(child, newName);
    }

    if((pip->INODE.i_mode & 0040000) != 0040000)
    {
        printf("Parent not a directory!\n");
        iput(pip);
        return -1;
    }

    if (search(pip, child) > 0)
    {
        printf("File already exists!\n");
        iput(pip);
        return -1;
    }

    created = k_creat(pip, child);
    
    ino = getino(&dev, newName);

    if (ino == -1)
        return -1;

    pip->refCount++;
    pip->INODE.i_links_count++;
    pip->INODE.i_atime = time(0);
    pip->dirty = 1;
    iput(pip);

    mip = iget(dev, ino);
    mip->INODE.i_links_count++;    
    mip->INODE.i_mode = 0xA1A4;
    memcpy((mip->INODE.i_block), oldName, strlen(oldName));
    mip->INODE.i_size = strlen(oldName);
    mip->dirty = 1;
    iput(mip);

    return created;
}

int my_unlink(char *pathname)
{
    int ino, pino;
    MINODE *pip, *mip;
    char name[64], *cp, parentDir[64];

    if (!pathname[0])
    {
        printf("Please enter file name!\n");
        return -1;
    }

    cp = strrchr(pathname, '/');
    
    if (cp == NULL)
    {
        pino = running->cwd->ino;
        strcpy(name, pathname);
    }

    else
    {
        *(cp) = '\0';
        strcpy(parentDir, pathname);
        pino = getino(dev, parentDir);
        strcpy(name, cp + 1);
    }

    ino = getino(dev, pathname);
    mip = iget(dev, ino);

    pip = iget(dev, pino);

    if (ino == -1 || pino == -1)
    {
        printf("File does not exist!\n");
        return -1;
    }

    if(!S_ISREG(mip->INODE.i_mode))
    {
        printf("File is not regular!\n");
        iput(pip);
        return -1;
    }
    
    mip->INODE.i_links_count--;

    /*if (mip->INODE.i_links_count > 1)
    {
        printf("File is busy!\n");
        iput(mip);
        return -1;
    }*/
    
    if (mip->INODE.i_links_count == 0)
    {
        truncate(mip);
        mip->refCount++;
        mip->dirty = 1;
        iput(mip);
        idealloc(dev, mip->ino);
    }

    else
    {
        pip->dirty = 1;
        iput(pip);
    }
    
    rmChild(pip, name);

    return 1;
}

int my_open(char *pathname, int fd)
{
    int flag, ino, i;
    MINODE *mip;
    char path[64], mode[64];
    if (!pathname[0])
    {
        printf("Please specify mode and file\n");
        return -1;
    }   
    
    if (fd == -1)
    {
        //path = strtok(pathname, " ");
        //mode = strtok(NULL, " ");
        sscanf(pathname, "%s %s", path, mode);
        
        if (!mode[0])
        {
            printf("Please specify mode and file\n");
            return -1;
        }
        
        flag = atoi(mode);
    }

    else
    {
        flag = fd;
        //path = (char *) malloc((strlen(pathname)) * sizeof(char));
        sscanf(pathname, "%s", path);
        //strncpy(path, pathname, strlen(pathname));
        //path[strlen(pathname)] = 0;
    }
    
    ino = getino(&dev, path);

    if (ino == -1)
    {
        printf("file does not exist!\n");
        return -1;
    }

    mip = iget(dev, ino);

    if (!S_ISREG(mip->INODE.i_mode))
    {
        printf("Must be a regular file!\n");
        return -1;
    }

    for (i = 0; i < NFD; i++)
    {
        if (running->fd[i] != 0)
        {
            if (running->fd[i]->inodeptr == mip)
            {
                if (running->fd[i]->mode > 0)
                {
                    printf("File is already open for write\n");
                    return -1;
                }
            }
        }
    }

    OFT *poft;
    poft = malloc(sizeof(OFT));
    i = new_oft(poft);
    
    if (i == -1)
        return -1;

    poft->mode = flag;
    poft->refCount = 1;
    poft->inodeptr = mip;

    if (flag == 0 | flag == 1 | flag == 2)
    {
        poft->offset = 0;
    }

    else if (flag == 3)
    {
        poft->offset = mip->INODE.i_size;
    }

    else
    {
        printf("Not a valid mode\n");
        running->fd[i] = 0;
        return -1;
    }

    mip->INODE.i_atime = mip->INODE.i_mtime = time(0L);
    mip->dirty = 1;
   
    iput(mip);
    return i;
}

int new_oft(OFT *poft)
{
    int i;

    for (i = 0; i < NOFT && running->fd[i] != NULL; i++);

    if (i == NOFT)
    {
        printf("No availabe fd\n");
        return -1;
    }

    running->fd[i] = poft;
    return i;
}

int write_file(char *pathname)
{
    if (!pathname[0])
    {
        printf("Please specify fd\n");
        return -1;
    }   
    
    int fd = atoi(pathname);
    
    if (fd < 0 || fd > NFD-1)
    {
        printf("Enter valid fd\n");
        return -1;
    }

    char input[1024];

    printf("Enter string to write: ");
    fgets(input, 1024, stdin);
    input[strlen(input) - 1] = 0;
    
   // OFT *oftp = running->fd[fd];
    if (running->fd[fd] != 0)
    {
        if (running->fd[fd]->mode == 0)
        {
            printf("not open for write\n");
            return -1;
        }
    }

    else
    {
        printf("not found\n");
        return -1;
    }


    int nbytes = strlen(input);
    printf("nbytes: %d\n", nbytes);

    return (mywrite(fd, input, nbytes));
}

int mywrite(int fd, char buf[ ], int nbytes)
{
    int blk, newblk, lblk2, lblk3, startByte, remain, nBytesTot;
    int lbk;
    OFT *oftp = running->fd[fd];
    MINODE *mip = oftp->inodeptr;
    int tempBuf1[256], tempBuf2[256];
    char tbuf[BLOCK_SIZE]; 
    char wBuf[BLOCK_SIZE];

    char *cp, *cq;

    cq = buf;

    while (nbytes > 0)
    {
        lbk = oftp->offset / BLOCK_SIZE;        
        startByte = oftp->offset % BLOCK_SIZE;
    
        if (lbk < 12)
        {
            if (mip->INODE.i_block[lbk] == 0)
            {
                blk = balloc(running->fd[fd]->inodeptr->dev);
                running->fd[fd]->inodeptr->INODE.i_block[lbk] = blk;
            }
            
            blk = mip->INODE.i_block[lbk];
        }

        else if (lbk >= 12 && lbk < 256 + 12) 
        {
             if(running->fd[fd]->inodeptr->INODE.i_block[12] == 0)              
             { 
                 newblk = balloc(running->fd[fd]->inodeptr->dev);
                 running->fd[fd]->inodeptr->INODE.i_block[12] = newblk;
             }

             memset((char*)tempBuf1, 0, BLOCK_SIZE);

             get_block(running->fd[fd]->inodeptr->dev, running->fd[fd]->inodeptr->INODE.i_block[12], (char*)tempBuf1);

             blk = tempBuf1[lbk-12];

             if (blk == 0)
             {
                 blk = balloc(running->fd[fd]->inodeptr->dev);
                 tempBuf1[lbk-12] = blk;
                 put_block(running->fd[fd]->inodeptr->dev, running->fd[fd]->inodeptr->INODE.i_block[12], (char*)tempBuf1); 
             }
        }

        else
        {
              if(running->fd[fd]->inodeptr->INODE.i_block[13] == 0)
              {
                  newblk = balloc(running->fd[fd]->inodeptr->dev);
                  running->fd[fd]->inodeptr->INODE.i_block[13] = newblk;
              }

              memset((char*)tempBuf1, 0, BLOCK_SIZE);
              get_block(running->fd[fd]->inodeptr->dev, running->fd[fd]->inodeptr->INODE.i_block[13], (char*)tempBuf1);
              lblk2 = (lbk - (256+12)) / 256;
              lblk3 = (lbk - (256+12)) % 256;
              if(tempBuf1[lblk2] == 0)

              {
                  newblk = balloc(running->fd[fd]->inodeptr->dev);
                  tempBuf1[lblk2] = newblk;
              }

              memset((char*)tempBuf2, 0, BLOCK_SIZE);
              get_block(running->fd[fd]->inodeptr->dev, tempBuf1[lblk2], (char*)tempBuf2);
              blk = tempBuf2[lblk3];

              if(blk == 0)
              {
                  blk = balloc(running->fd[fd]->inodeptr->dev);
                  tempBuf2[lblk3] = blk;
                  put_block(running->fd[fd]->inodeptr->dev, tempBuf1[lblk2], (char*)tempBuf2);
              }

              put_block(running->fd[fd]->inodeptr->dev, running->fd[fd]->inodeptr->INODE.i_block[13], (char*)tempBuf1);
        }

        memset(tbuf, 0, 1024);
        get_block(mip->dev, blk, tbuf); 
        
        cp = tbuf + startByte;  
        remain = BLOCK_SIZE - startByte;
        
        while (remain > 0)
        {
            *cp++ = *cq++;
            nbytes--;
            remain--;
            oftp->offset++;


            if (oftp->offset > mip->INODE.i_size)
                mip->INODE.i_size++;

            if (nbytes <= 0)
                break;
        }

        put_block(mip->dev, blk, tbuf);
    } 

    mip->dirty = 1;       // mark mip dirty for iput() 
    iput(mip);
    printf("Wrote %d char into file descriptor fd=%d\n", nbytes, fd);           
    
    return nbytes;
}

int read_file(char *pathname, int byte)
{
    char buf[BLOCK_SIZE];

    if (!pathname[0])
    {
        printf("Please specify fd\n");
        return -1;
    }   
   
    int fd, nbytes;
    
    if (byte == -1)
        sscanf(pathname, "%d %d", &fd, &nbytes); 
    
    else
    {
        fd = atoi(pathname);
        nbytes = byte;
    }

    if (fd < 0)
    {
        printf("Enter valid fd\n");
        return -1;
    }

    if (fd < 0 || fd > NFD - 1)
    {
        printf("Please enter a valid fd\n");
        return -1;
    }

    if (running->fd[fd] == NULL)
    {
        printf("Can't find fd\n");
        return -1;
    }

    if (running->fd[fd]->mode != 0 && running->fd[fd]->mode != 2)
    {
        printf("File is not open for read\n");
        return -1;
    }
    
    char input[1024];

    printf("Enter how many bytes you want to read: ");
    fgets(input, 1024, stdin);
    input[strlen(input) - 1] = 0;
    sscanf(input, "%d", &nbytes);
    memset(buf, 0, BLOCK_SIZE); 
    myread(fd, buf, nbytes);

    printf("*****************\n");
    printf("%s\n", buf);
    printf("*****************\n");

    return 0;
}

int myread(int fd, char buf[], int nbytes)
{
    char path[5] = "f1";
    MINODE *mip = running->fd[fd]->inodeptr;
    int avil = mip->INODE.i_size - running->fd[fd]->offset;
    char tbuf[BLOCK_SIZE], dbuf[BLOCK_SIZE];
    long lbk, startByte, prevlbk = -1;
    int blk, count = 0;
    char *cq = buf;
    long *indirect, *dindirect;
    int id = 0, did = 0, flag = 0;
    while (nbytes && avil)
    {
        lbk = running->fd[fd]->offset / BLOCK_SIZE;
        startByte = running->fd[fd]->offset % BLOCK_SIZE;

        if (lbk < 12)
            blk = running->fd[fd]->inodeptr->INODE.i_block[lbk]; 
        
        else if (lbk >= 12 && lbk < 256 + 12)
        {
            int ind = mip->INODE.i_block[12];
                   
            if(!ind)
                return 0;

            int membuf[BLOCK_SIZE/4];
            get_block(mip->dev, ind, (char *)membuf);
            int blkp = (lbk-12);

            if(!membuf[blkp])
                return 0;

            blk = membuf[blkp];
        }

        else
        {
            strcpy(tbuf, "");
            int dlbk = lbk - 256 - 12;
            int dind = (dlbk / 256);
            int ind = dlbk % 256;
            int ptr1 = mip->INODE.i_block[13];
            if(!ptr1)               
            {
                return 0;
            }
            int *ptr;
            int membuf[BLOCK_SIZE/4];
            get_block(mip->dev, ptr1, membuf);
            ptr = tbuf + dind*4;

            if(!membuf[dind])                
                return 0;

            ptr1 = membuf[dind];
            get_block(mip->dev, ptr1, membuf);
            ptr = tbuf + ind*4;

            if(!membuf[ind])
                return 0;
            
            blk = membuf[ind];
        }

        get_block(running->fd[fd]->inodeptr->dev, blk, readBuf);
        char *cp = readBuf + startByte; 
        int remain = BLOCK_SIZE - startByte;
    
        while (remain > 0)
        {
            *cq++ = *cp++;
            running->fd[fd]->offset++;
            count++;
            avil--;
            nbytes--;
            remain--;

            if (nbytes <= 0 || avil <= 0)
                break;
       }
    }

    return count;   // count is the actual number of bytes read
}

int close_file(char *pathname)
{
    MINODE *mip;

    if (!pathname[0])
    {
        printf("Please enter a fd\n");
        return -1;
    }

    int fd = atoi(pathname);
    
    if (fd < 0 || fd >= NFD)
    {
        printf("Not a valid fd\n");
        return -1;
    } 

    if (running->fd[fd] == NULL)
    {
        printf("Unable to find specified fd\n");
        return -1;
    }

    OFT *oftp = running->fd[fd];
    running->fd[fd] = 0;
    oftp->refCount--;

    if (oftp->refCount > 0)
        return -1;

    else
    {
        mip = oftp->inodeptr;
        iput(mip);
    }

    return 0;
}

int my_cat(char *pathname)
{
    char buf[1024], dummy = 0;
    int n;
    int fd = my_open(pathname, 0);
    char input[128];

    sprintf(input, "%d", fd);
    printf("******************************\n"); 
    while (n = myread(fd, buf, BLOCK_SIZE))
    {
        buf[n] = 0;
        printf("%s", buf);
    }

    printf("\n");
    printf("******************************\n"); 
    close_file(input);

    return 0;
}

int my_cp(char *pathname)
{
    char source[128], dest[128];
    sscanf(pathname, "%s %s", source, dest);
    MINODE *mip = running->cwd;
    if (search(mip, dest) < 1)
        my_creat(dest);

    my_touch(dest);
    int fd = my_open(source, 0);
    int gd = my_open(dest, 1);
    int n;
    char buf[BLOCK_SIZE], copybuf[BLOCK_SIZE];
    
    if (fd == -1)
    {
        printf("Couldn't open source fd\n");
        return -1;
    }

    if (gd == -1)
    {
        printf("Couldn't open destination fd\n");
        return -1;
    }
    
    while (n = myread(fd, copybuf, BLOCK_SIZE))
    {
        printf("read\n");
        mywrite(gd, copybuf, n);
        memset(copybuf, 0, BLOCK_SIZE);
    }

    char cfd[64], cgd[64]; 
    sprintf(cfd, "%d", fd);
    sprintf(cgd, "%d", gd);
    close_file(cgd);
    close_file(cfd);

    return 0;
}

int my_mv(char *pathname)
{
    char source[128], dest[128];
    sscanf(pathname, "%s %s", source, dest);
    int ino = getino(dev, source);
    MINODE *mip = iget(dev, ino);//running->fd[sfd]->inodeptr;
    
    if (mip->dev == fd)
    {
        my_link(pathname);
        cd("");
        my_unlink(source);
    }

    else
    {
        close_file(source);
        my_cp(pathname);
        my_unlink(source);
    }
    return;
}

int my_lseek(char *pathname)
{
    int fd;
    int place;
    
    sscanf(pathname, "%d %d", &fd, &place);

    if (fd < 0)
    {
        printf("not a valid fd\n");
        return -1;
    }

    if (place > running->fd[fd]->inodeptr->INODE.i_size)
    {
        printf("File not long enough\n");
        return -1;
    }

    else if (place < 0)
    {
        printf("Have to seek a positive number\n");
        return -1;
    }

    long offset = running->fd[fd]->offset;
    running->fd[fd]->offset = place;
    return offset;
}

int my_mount(char *pathname)
{
    char file[64], path[64], buf[BLOCK_SIZE];
    int ino, i, mfd;
    MINODE *mip;

    memset(file, 0, 64);
    memset(path, 0, 64);

    sscanf(pathname, "%s %s", file, path);

    if (!file[0] || !path[0])
    {
        printf("Please enter disk and mount point!\n");
        return -1;
    }

    for (i = 0; mtable[i].dev != 0 && i < NMOUNT; i++)
    {
        if (strcmp(mtable[i].name, file) == 0)
        {
            printf("Disk already mounted\n");
            return -1;
        }
    }
    
    if ((mfd = open(file, O_RDWR)) < 0)
    {
        printf("Open disk %s failed\n", disk);
        return -1;
    }

    get_block(mfd, 1, buf);
    SUPER *sp = (SUPER *) buf;

    if (sp->s_magic != 0xEF53)
    {
        printf("Magic %x is not an EXT2 FS\n", sp->s_magic);
        close(file);
        return -1;
    }

    ino = getino(&dev, path);
    mip = iget(dev, ino);

    if (!S_ISDIR(mip->INODE.i_mode))
    {
        printf("Can't mount to a non directory!\n");
        close(file);
        return -1;
    }

    if (mip->refCount > 2)
    {
        printf("Mount point is busy!\n");
        close(file);
        return -1;
    }
    
    mtable[i].dev = mfd;
    mtable[i].mounted_inode = mip;

    mip->mounted = 1;
    mip->mountptr = &mtable[i];
    mtable[i].mounted_inode = mip;
    strncpy(mtable[i].name, file, strlen(file));
    strncpy(mtable[i].mount_name, path, strlen(path));

    mtable[i].ninodes = sp->s_inodes_count;
    mtable[i].nblocks = sp->s_blocks_count;

    get_block(mfd, 2, buf);
    gp = (GD *) buf;

    mtable[i].bmap = gp->bg_block_bitmap;    
    mtable[i].imap = gp->bg_inode_bitmap;
    mtable[i].iblk = gp->bg_inode_table;

    if (i != 0)
    {
        switch_dev(i);
    }

    MINODE *newmnt = iget(mtable[i].dev, 2);

    if (i != 0)
    {
        int j;

        for (j = 0; mtable[j].dev != running->cwd->dev; j++);
        switch_dev(j);
    }
    newmnt->dirty = 1;
    iput(newmnt);

    mip->dirty = 1;
    iput(mip);

    printf("mounted %s on %s\n", name, path);
    
    return 0;
}

int switch_dev(int disk)
{
    fd = mtable[disk].dev;
    dev = mtable[disk].dev;
    nblocks = mtable[disk].nblocks;
    ninodes = mtable[disk].ninodes;
    bmap = mtable[disk].bmap;
    imap = mtable[disk].imap;
    inode_start = mtable[disk].iblk;
}

int my_umount(char *pathname)
{
    int i, j, busy;

    for(i = 0; i < NFD && (strncmp(mtable[i].name, pathname, strlen(pathname)) != 0); i++);

    if (i == NFD)
    {
        printf("Disk %s not mounted\n", pathname);
        return -1;
    }

    MINODE *mip = mtable[i].mounted_inode;

    for (j = 0; j < NMINODE; j++)
    {
        if (minode[j].dev == mtable[i].dev && minode[j].refCount != 0)
            busy++;
    }

    if (busy > 0)
        return -1;

    mip->mounted = 0;
    mip->mountptr = NULL;
    mtable[i].dev = 0;

    mip->dirty = 1;
    iput(mip);

    printf("%s unmounted\n", pathname);

    return 0;
}
