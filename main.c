
#include <string.h>

#include "sfs.h"

int main(int argc, char* argv[]) {
    
    char write_buffer[] = "Wothfak u sajd tu mí jů litr bich?!";
    char* read_buffer   = calloc(100, 1);
    
    format_sfs("disk.sfs", BLOCK_SIZE * 4);
    
    open_sfs("disk.sfs");
    
    printf("Disk info: [blocks: %u] [inode blocks: %u] [inodes: %u]\n", sfs.blocks, sfs.inode_blocks, sfs.inodes);
    
    
    
    sfs_file* out = sfs_open_file(0, SFS_MODE_WRITE);
    
    sfs_write_file(write_buffer, strlen(write_buffer), out);
    
    sfs_write_file(write_buffer, strlen(write_buffer), out);
    
    sfs_close_file(out);
    
    sfs_file* in = sfs_open_file(0, SFS_MODE_READ);
    
    sfs_read_file(read_buffer, 80, in);
    
    printf("%s\n", read_buffer);
    
    sfs_close_file(in);
    
    
    close_sfs();
    
}
