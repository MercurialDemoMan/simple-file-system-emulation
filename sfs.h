
//note: cannot use c pointers because of their 8-byte width

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>

#define BLOCK_SIZE             0x1000
#define MAGIC_NUMBER           0xf0f03410

#define GET_BIT(x,y)           ((x>>y)&1U)
#define SET_BIT(x,y,z)         x^=(-!!z^x)&(1U<<y)

#define MAX_FILE_SIZE		   (5*BLOCK_SIZE+1024*BLOCK_SIZE)

#define SFS_MODE_READ          0
#define SFS_MODE_WRITE         1

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

/*DISK IMPLEMENTATION*/

//init with init_sfs
char* g_free_block_bitmap;

typedef struct SFS {
    u32 magic;        //sfs header
    u32 blocks;       //number of blocks
    u32 inode_blocks; //number of blocks set aside for storing inodes, 10% of total blocks rounding up
    u32 inodes;       //number of inodes in inodes blocks
    
    FILE* disk;
    
    //remainder of disk block is filled with 0
    //could be used for free blocks bitmap
    
} SFS;

static SFS sfs;

typedef struct inode {
    u32 valid;       //1 - has been created, 0 - not
    u32 size;        //size of data in inode
    u32 direct[5];   //direct pointers to data blocks
    u32 indirect;    //indirect pointer to poiters to data
} inode;

typedef struct file {
    inode node;
    u32   data_pointer;
    u32   inumber;
} sfs_file;



void format_sfs(char* emu_disk_file, u32 disk_size); //erases disk and fills it with zeros
void open_sfs  (char* emu_disk_file);                //opens and recalculates free block bitmap
void close_sfs ();

u32 read_block (void* buffer, u32 block_index, u32 size);
u32 write_block(void* buffer, u32 block_index, u32 size);

u32 get_free_node();



/*FILE IMPLEMENTATION*/

sfs_file* sfs_open_file (u32 index, u8 mode);
void sfs_delet_file(u32 index);
void sfs_close_file(sfs_file* file);

u32  sfs_read_file (void* buffer, u32 size, sfs_file* file);
u32  sfs_write_file(void* buffer, u32 size, sfs_file* file);

u32  sfs_file_size (sfs_file* file);
void sfs_file_seek (sfs_file* file, u32 offset);
u32  sfs_file_tell (sfs_file* file);

