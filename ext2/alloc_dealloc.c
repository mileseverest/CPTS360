#include "type.h"

/*********** alloc_dealloc.c file *************/

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

unsigned long balloc(int dev)
{
	int bitpos;
	char buf[1024];
	get_block(dev, BBITMAP, buf);
	for (bitpos=0; bitpos < NBLOCKS; bitpos++)
	{
	   if (TST_bit(buf, bitpos)==0)
	   {
		   SET_bit(buf, bitpos);
		   put_block(dev, BBITMAP, buf);
		   decFreeInodes(dev);
		   return bitpos+1;
	   }
	}

	return 0;
}


int bdealloc(int dev, int bit)
{
	int i;  
	char buf[BLOCK_SIZE];
	get_block(dev, BBITMAP, buf);
	CLR_bit(buf, bit-1);
	put_block(dev, BBITMAP, buf);
	incFreeBlocks(dev);
}
