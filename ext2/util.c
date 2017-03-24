//#include "type.h"

void get_block(int fd, int blk, char buf[BLOCK_SIZE])
{
//    printf("Block: %ld\n", (long)blk*BLOCK_SIZE);

    if (lseek(fd, (long)(blk * BLOCK_SIZE), 0) == -1)
        printf("ERROR\n");
    read(fd, buf, BLOCK_SIZE);
}

void put_block(int fd, int blk, char buf[BLOCK_SIZE])
{ 
    lseek(fd, (long)(blk * BLOCK_SIZE), 0); 
    write(fd, buf, BLOCK_SIZE);
}

char **tokenize(char *pathname, char delim)
{
    int i = 0;
    char** name;
    char* tmp;

    name = (char**)malloc(sizeof(char*)*256);
    name[0] = strtok(pathname, "/");
    i = 1;

    while ((name[i] = strtok(NULL, "/")) != NULL) 
        i++;

    name[i] = 0;

    i = 0;

    while(name[i])
    {
        tmp = (char*)malloc(sizeof(char)*strlen(name[i]));
        strcpy(tmp, name[i]);
        name[i] = tmp;
        i++;
    }

    return name;
}


char *dirname(char *pathname)
{
    int i = strlen(pathname) - 1;

    char *dir;

    while(pathname[i] != '/' && i >= 0)
        i--;

    if (i < 0)
        i = 0;
    
    if (!i)
    {
        dir = (char *)malloc(2*sizeof(char));

        strcpy(dir, "/");
    }

    else
    {
        dir = (char *)malloc((i+1)*sizeof(char));

        strncpy(dir, pathname, i);
        dir[i] = 0;
    }

    return dir;
}

char *basename(char *input)
{
    char *output, **temp;

    temp = tokenize(input, "/");

    int i, j;

    for (i = 0; temp[i] != '\0'; i++);
    i--;
    
    int baseLength = strlen(temp[i]);

    output = malloc(sizeof(char) *(baseLength));

    output = temp[i];

    return output;  
}







