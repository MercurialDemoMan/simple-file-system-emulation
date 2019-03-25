#include "sfs.h"

#define SFS_ERROR(x)           printf(x); return
#define SFS_NULL_ERROR(x)      printf(x); return NULL
#define SFS_ZERO_ERROR(x)      printf(x); return 0

#define SFS_NULL               0

#define MY_DEBUG               printf

/*DISK IMPLEMENTATION*/

//format simple file system
void format_sfs(char* emu_disk_file, u32 disk_size) {
    
    //check disk size validity
    if(disk_size % BLOCK_SIZE != 0)
    {
        SFS_ERROR("format_sfs() error: disk size is not multiple of 4096\n");
    }
    if(disk_size < BLOCK_SIZE * 3)
    {
        SFS_ERROR("format_sfs() error: disk size is not sufficient enough (note: must be bigger than 3 * 4096 bytes\n");
    }
    
    //setup sfs disk
    sfs.disk         = fopen(emu_disk_file, "wb"); if(sfs.disk == NULL) { SFS_ERROR("format_sfs error: cannot open emulated drive\n"); }
    sfs.magic        = MAGIC_NUMBER;
    sfs.blocks       = disk_size / BLOCK_SIZE;
    sfs.inode_blocks = ceil(sfs.blocks * 0.1);
    sfs.inodes       = sfs.inode_blocks * BLOCK_SIZE / sizeof(inode);
    
    //write sfs header
    fwrite((char*)&sfs, sizeof(sfs) - sizeof(FILE*), 1, sfs.disk);
    
    //fill rest of the emulated disk with zeros
    char* buffer = calloc(BLOCK_SIZE, sizeof(char));
    
    //fill rest of the first block
    fwrite(buffer, sizeof(char), BLOCK_SIZE - sizeof(sfs) + sizeof(FILE*), sfs.disk);
    
    //fill rest of the blocks with zeros
    for(u32 i = 0; i < sfs.blocks; i++)
    {
        fwrite(buffer, sizeof(char), BLOCK_SIZE, sfs.disk);
    }
    
    free(buffer);
    
    fclose(sfs.disk);
}

//open disk
void open_sfs(char* emu_disk_file) {
    
    sfs.disk = fopen(emu_disk_file, "r+b");
    
    //read super block
    fread((char*)&sfs, sizeof(sfs) - sizeof(FILE*), 1, sfs.disk);
    
    //check header
    if(sfs.magic != MAGIC_NUMBER)
    {
        SFS_ERROR("open_sfs error: read disk is not simple file system formatted\n");
    }
    
    //allocate header block and inodes blocks
    g_free_block_bitmap = calloc((sfs.blocks - 1) / 8 + 1, sizeof(char));
    
    SET_BIT(g_free_block_bitmap[0], 0, 1);
    
    for(u32 i = 1; i < sfs.inode_blocks + 1; i++)
    {
        SET_BIT(g_free_block_bitmap[i / 8], i % 8, 1);
    }
    
    //scan inodes for allocated blocks
    fseek(sfs.disk, BLOCK_SIZE, SEEK_SET);
    
    inode node;
    
    for(u32 i = 0; i < sfs.inode_blocks; i++) {
        
        for(u32 j = 0; j < BLOCK_SIZE / sizeof(inode); j++) {
            
            //load node
            fread((char*)&node, sizeof(node), 1, sfs.disk);
            
            //check node allocation
            if(node.valid) {
                
                //scan direct links
                for(u32 k = 0; k < 5; k++) {
                    
                    if(node.direct[k] != SFS_NULL) {
                        
                        if(node.direct[k] % BLOCK_SIZE != 0) {
                            SFS_ERROR("open_sfs error: node direct pointer points to invalid block, system corrupted\n");
                        }
                        
                        u32 block_index = node.direct[k] / BLOCK_SIZE;
                        
                        SET_BIT(g_free_block_bitmap[block_index / 8], block_index % 8, 1);
                        
                    }
                }
                
                //scan indirect link
                if(node.indirect != SFS_NULL) {
                    
                    if(node.indirect % BLOCK_SIZE != 0) {
                        SFS_ERROR("open_sfs error: node indirect pointer points to invalid block, system corrupted\n");
                    }
                    
                    //save file pointer
                    //don't know what is this for :D
                    //u32 node_add = ftell(sfs.disk);
                    
                    //set file pointer to the indirect block
                    fseek(sfs.disk, node.indirect, SEEK_SET);
                    
                    //read pointers and allocate blocks
                    u32   indirect_pointer;
                    
                    for(u32 k = 0; k < BLOCK_SIZE / sizeof(u32); k += 1) {
                        
                        fread((char*)&indirect_pointer, sizeof(u32), 1, sfs.disk);
                        
                        if(indirect_pointer != SFS_NULL) {
                            
                            if(indirect_pointer % BLOCK_SIZE != 0) {
                                SFS_ERROR("open_sfs error: node indirect pointer in indirect array points to invalid block, system corrupted\n");
                            }
                            
                            u32 block_index = indirect_pointer / BLOCK_SIZE;
                            
                            SET_BIT(g_free_block_bitmap[block_index / 8], block_index % 8, 1);
                            
                        }
                    }
                }
            }
        }
    }
}

//close disk
void close_sfs() {
    
    fclose(sfs.disk);
    free(g_free_block_bitmap);
}

//read block
u32 read_block(void* buffer, u32 block_index, u32 size) {

    if(block_index + 1 > sfs.blocks) {
        SFS_ZERO_ERROR("read_block error: block index out of range\n");
    }

    if(size > BLOCK_SIZE) {
        SFS_ZERO_ERROR("read_block error: buffer size is bigger than block size\n");
    }

    fseek(sfs.disk, block_index * BLOCK_SIZE, SEEK_SET);

    return fread(buffer, sizeof(char), size, sfs.disk);
}

u32 write_block(void* buffer, u32 block_index, u32 size) {

    if(block_index + 1 > sfs.blocks) {
        SFS_ZERO_ERROR("write_block error: block index out of range\n");
    }

    if(size > BLOCK_SIZE) {
        SFS_ZERO_ERROR("write_block error: buffer size is bigger than block size\n");
    }

    fseek(sfs.disk, block_index * BLOCK_SIZE, SEEK_SET);

    return fwrite(buffer, sizeof(char), size, sfs.disk);
}

u32 get_free_node() {

    for(u32 i = 0; i < (sfs.blocks - 1) / 8 + 1; i++) {

        for(u8 j = 0; j < 8; j++) {

            if(i * 8 + j + 1 > sfs.blocks) {

                SFS_ZERO_ERROR("get_free_node error: out of physical memory\n");
            }

            //if node free return it's index
            if(!GET_BIT(g_free_block_bitmap[i], j)) {
                
                return i * 8 + j;
            }
        }
    }

    SFS_ZERO_ERROR("get_free_node error: out of physical memory\n");
}

/*FILE IMPLEMENTATION*/

//TODO: implement modes, now only supporing "wb"
//opens file if doesn't exist create it
sfs_file* sfs_open_file (u32 index, u8 mode) {
    
    if(index > sfs.inodes) {
        SFS_NULL_ERROR("sfs_open_file error: index out of range\n");
    }

    u32 node_index = BLOCK_SIZE + index * sizeof(inode);

    //open desired node
    fseek(sfs.disk, node_index, SEEK_SET);

    inode node;

    fread((char*)&node, sizeof(node), 1, sfs.disk);

    //create file
    sfs_file* file = malloc(sizeof(sfs_file));
    
    //check mode
    if(!node.valid && mode == SFS_MODE_READ) {
        SFS_NULL_ERROR("sfs_open_file error: file doesn't exist\n");
    }
    
    //reset the node
    if(!node.valid || mode == SFS_MODE_WRITE) {
        
        sfs_delet_file(index);
        
        node.valid = 1;
        node.size  = 0;
        node.direct[0] = SFS_NULL;
        node.direct[1] = SFS_NULL;
        node.direct[2] = SFS_NULL;
        node.direct[3] = SFS_NULL;
        node.direct[4] = SFS_NULL;
        node.indirect  = SFS_NULL;

        fseek(sfs.disk, node_index, SEEK_SET);
        fwrite((char*)&node, sizeof(node), 1, sfs.disk);
    }
    
    file->data_pointer = 0;
    file->node         = node;
    file->inumber      = index;

    return file;
}

//closes file
void sfs_close_file(sfs_file* file) {

    free(file);
    file = NULL;
}

//read file
u32  sfs_read_file (void* buffer, u32 size, sfs_file* file) {

    if(size == 0) { return 0; }

    if(size > file->node.size) {
        size = file->node.size;
    }
    
    //create backup
    sfs_file* file_copy = malloc(sizeof(sfs_file));
    memcpy(file_copy, file, sizeof(sfs_file));

    //create necessary variables
    u32 data_index  = file->data_pointer / BLOCK_SIZE;
    u32 data_offset = file->data_pointer % BLOCK_SIZE;
    
    u32 remaining_space_in_block = (data_offset == 0) ? 0 : BLOCK_SIZE - data_offset;
    
    u32 blocks_num  = floor(((double)size - (double)remaining_space_in_block - 1.0) / (double)BLOCK_SIZE + 1.0);
    
    u32 reminder    = (size - remaining_space_in_block) % BLOCK_SIZE;
    u32 bytes_read  = 0;

    char* block_buffer   = malloc(BLOCK_SIZE);
    char* buffer_pointer = (char*)buffer;
    
    u32 temp_block_index;
    
    //if there is space in block read it
    if(remaining_space_in_block != 0) {
        
        //find the block which data pointer points to
        //in direct blocks
        if(data_index < 5) {
            
            temp_block_index = file_copy->node.direct[data_index] / BLOCK_SIZE;
            
        //indirect blocks
        } else {
            
            read_block(block_buffer, file_copy->node.indirect / BLOCK_SIZE, BLOCK_SIZE);
            
            temp_block_index = *(u32*)(block_buffer + (data_index - 5) * sizeof(u32)) / BLOCK_SIZE;
            
        }
        
        //if the data will fit into that remaining space
        if(blocks_num == 0) {
            
            read_block(block_buffer, temp_block_index, BLOCK_SIZE);
            
            memcpy(buffer_pointer, block_buffer + data_offset, size);
            
            bytes_read              += size;
            file_copy->data_pointer += size;
            
        //we read the that remaining data and the proceed onto the next blocks
        } else {
            
            read_block(block_buffer, temp_block_index, BLOCK_SIZE);
            
            memcpy(buffer_pointer, block_buffer + data_offset, remaining_space_in_block);
            
            bytes_read              += remaining_space_in_block;
            buffer_pointer          += remaining_space_in_block;
            file_copy->data_pointer += remaining_space_in_block;
            
            data_index        = file_copy->data_pointer / BLOCK_SIZE;
            data_offset       = file_copy->data_pointer % BLOCK_SIZE;
            
        }
        
    }
    
    
    for(u32 i = 0; i < blocks_num; i++) {
        
        //find the block which data pointer points to
        //in direct blocks
        if(data_index < 5) {
            
            temp_block_index = file_copy->node.direct[data_index] / BLOCK_SIZE;
            
        //indirect blocks
        } else {
            
            read_block(block_buffer, file_copy->node.indirect / BLOCK_SIZE, BLOCK_SIZE);
            
            temp_block_index = *(u32*)(block_buffer + (data_index - 5) * sizeof(u32)) / BLOCK_SIZE;
            
        }
        
        //read the block
        read_block(block_buffer, temp_block_index, BLOCK_SIZE);
        
        //ending block
        if(i == blocks_num - 1) {
            
            //if data is block aligned
            if(reminder == 0) {
                
                bytes_read              += read_block(buffer_pointer, temp_block_index, BLOCK_SIZE);
                file_copy->data_pointer += BLOCK_SIZE;
                
                //if not
            } else {
                
                bytes_read              += read_block(buffer_pointer, temp_block_index, reminder);
                file_copy->data_pointer += reminder;
            }
            
            //this whole shit could be done like:
            //bytes_read              += read_block(buffer_pointer, temp_block_index, (reminder == 0) ? BLOCK_SIZE : reminder);
            //file_copy->data_pointer += (reminder == 0) ? BLOCK_SIZE : reminder;
            
        //middle block
        } else {
            
            bytes_read              += read_block(block_buffer, temp_block_index, BLOCK_SIZE);
            
            buffer_pointer          += BLOCK_SIZE;
            file_copy->data_pointer += BLOCK_SIZE;
            
        }
        
        data_index = file_copy->data_pointer / BLOCK_SIZE;
    }
    
    //flush the file
    fseek(sfs.disk, BLOCK_SIZE + file->inumber * sizeof(inode), SEEK_SET);
    
    fwrite((char*)&file_copy->node, sizeof(inode), 1, sfs.disk);
    
    memcpy(file, file_copy, sizeof(sfs_file));
    
    free(file_copy);
    free(block_buffer);

    return bytes_read;
}

//write file
u32  sfs_write_file(void* buffer, u32 size, sfs_file* file) {
    
    if(size == 0) { return 0; }

    //note: no cotrol of overflow
    if(size + file->node.size > MAX_FILE_SIZE) {
        SFS_ZERO_ERROR("sfs_write_file error: size of data is too big\n");
    }

    //create backup
    sfs_file* file_copy = malloc(sizeof(sfs_file));
    memcpy(file_copy, file, sizeof(sfs_file));

    //create necessary variables
    u32 data_index        = file_copy->node.size / BLOCK_SIZE;
    u32 data_offset       = file_copy->node.size % BLOCK_SIZE;
    
    u32 remaining_space_in_last_block = (data_offset == 0) ? 0 : BLOCK_SIZE - data_offset;
    
    u32 blocks_num        = floor(((double)size - (double)remaining_space_in_last_block - 1.0) / (double)BLOCK_SIZE + 1.0);
    
    u32 reminder          = (size - remaining_space_in_last_block) % BLOCK_SIZE;
    u32 bytes_written     = 0;
    
    u32* blocks_array     = calloc(blocks_num, sizeof(u32));
    char* block_buffer    = malloc(BLOCK_SIZE);
    char* buffer_pointer  = buffer;
    
    //scan for free blocks
    for(u32 i = 0; i < blocks_num; i++) {
        
        blocks_array[i] = get_free_node();
        
        if(blocks_array[i] == SFS_NULL) {
            free(file_copy);
            free(blocks_array);
            free(block_buffer);
            SFS_ZERO_ERROR("sfs_write_file error: out of physical memory\n");
        }
     
    }
    
    //if there is space in the last block fill it
    //this will not run when the file is empty
    if(remaining_space_in_last_block != 0) {
        
        u32 single_block_index;
        
        //find the last block
        //in direct blocks
        if(data_index < 5) {
    
            single_block_index = file_copy->node.direct[data_index] / BLOCK_SIZE;
            
        //indirect blocks
        } else {
            
            read_block(block_buffer, file_copy->node.indirect / BLOCK_SIZE, BLOCK_SIZE);
            
            single_block_index = *(u32*)(block_buffer + (data_index - 5) * sizeof(u32)) / BLOCK_SIZE;
            
        }
        
        
        
        //if the data will fit into that remaining space
        if(blocks_num == 0) {
            
            read_block(block_buffer, single_block_index, BLOCK_SIZE);
            
            memcpy(block_buffer + data_offset, buffer_pointer, size);
            
            bytes_written        += write_block(block_buffer, single_block_index, BLOCK_SIZE);
            file_copy->node.size += size;
            
            //we are done, nope :/ actually probably yes
            
        //we write the that remaining data and the proceed onto the next blocks
        } else {
            
            read_block(block_buffer, single_block_index, BLOCK_SIZE);
            
            memcpy(block_buffer + data_offset, buffer_pointer, remaining_space_in_last_block);
            
            bytes_written        += write_block(block_buffer, single_block_index, BLOCK_SIZE);
            
            buffer_pointer       += remaining_space_in_last_block;
            file_copy->node.size += remaining_space_in_last_block;
            
            data_index            = file_copy->node.size / BLOCK_SIZE;
            data_offset           = file_copy->node.size % BLOCK_SIZE;
            
        }
    }

    //write the rest of the blocks
    //we are starting at the beggining of the block
    //be aware of the ending block
    //be aware of the direct and indirect pointers
    
    for(u32 i = 0; i < blocks_num; i++) {
        
        //scan inode for direct free block pointers
        //and set it to the current block
        bool found_direct = false;
        
        
        for(u32 j = 0; j < 5; j++) {
            
            if(file_copy->node.direct[j] == SFS_NULL) {
                
                file_copy->node.direct[j] = blocks_array[i] * BLOCK_SIZE;
                
                found_direct = true;
                break;
            }
        }
        
        //if not found scan for indirect free block pointers
        //if indirect block doesn't exist create it
        if(!found_direct) {
            
            if(file_copy->node.indirect == SFS_NULL) {
                
                file_copy->node.indirect = get_free_node() * BLOCK_SIZE;
                
                if(file_copy->node.indirect == SFS_NULL) {
                    
                    free(file_copy);
                    free(blocks_array);
                    free(block_buffer);
                    SFS_ZERO_ERROR("sfs_write_file error: indirect block cannot be allocated, out of memory\n");
                }
                
                //fill it with zeros
                memset(block_buffer, 0, BLOCK_SIZE);
                
                write_block(block_buffer, file_copy->node.indirect / BLOCK_SIZE, BLOCK_SIZE);
            }
            
            //scanning
            read_block(block_buffer, file_copy->node.indirect / BLOCK_SIZE, BLOCK_SIZE);
            
            bool found_indirect = false;
            
            for(u32 j = 0; j < BLOCK_SIZE; j += sizeof(u32)) {
                
                u32* p = (u32*)(block_buffer + j);
                
                //we found indirect pointer
                if(*p == SFS_NULL) {
                    
                    *p = blocks_array[i] * BLOCK_SIZE;
                    
                    write_block(block_buffer, file_copy->node.indirect / BLOCK_SIZE, BLOCK_SIZE);
                    
                }
                
            }
            
            if(!found_indirect)
            {
                free(file_copy);
                free(blocks_array);
                free(block_buffer);
                SFS_ZERO_ERROR("sfs_write_file error: free indirect block pointer was not found, out of memory\n");
            }
        }
        
        //ending block
        if(i == blocks_num - 1) {
            
            //if data is block aligned
            if(reminder == 0) {

                bytes_written        += write_block(buffer_pointer, blocks_array[i], BLOCK_SIZE);
                file_copy->node.size += BLOCK_SIZE;
                
             //if not
            } else {
            
                bytes_written        += write_block(buffer_pointer, blocks_array[i], reminder);
                file_copy->node.size += reminder;
            }
            
        //middle whole block
        } else {
            
            bytes_written        += write_block(buffer_pointer, blocks_array[i], BLOCK_SIZE);
            
            buffer_pointer       += BLOCK_SIZE;
            file_copy->node.size += BLOCK_SIZE;
            
        }
        
        data_index = file_copy->node.size / BLOCK_SIZE;
        
        SET_BIT(g_free_block_bitmap[(blocks_array[i] / BLOCK_SIZE) / 8], (blocks_array[i] / BLOCK_SIZE) % 8, 1);
    }
    
    //flush the file
    fseek(sfs.disk, BLOCK_SIZE + file->inumber * sizeof(inode), SEEK_SET);
    
    fwrite((char*)&file_copy->node, sizeof(inode), 1, sfs.disk);
    
    memcpy(file, file_copy, sizeof(sfs_file));
    
    free(file_copy);
    free(blocks_array);
    free(block_buffer);
    
    return bytes_written;
}

//delete inode
void sfs_delet_file(u32 index) {

    if(index > sfs.inodes) {
        SFS_ERROR("sfs_delet_file error: index out of bounds\n");
    }

    //load the node
    fseek(sfs.disk, BLOCK_SIZE + index * sizeof(inode), SEEK_SET);
    
    inode node;
    
    fread((char*)&node, sizeof(node), 1, sfs.disk);
    
    //if node is not active ignore everything
    if(!node.valid) { return; }
    
    //scan the pointers and deallocate them
    //direct
    for(u8 i = 0; i < 5; i++) {
        //deallocate the block
        if(node.direct[i] != SFS_NULL) {
            
            SET_BIT(g_free_block_bitmap[(node.direct[i] / BLOCK_SIZE) / 8], (node.direct[i] / BLOCK_SIZE) % 8, 0);
        }
    }
    
    //indirect
    if(node.indirect != SFS_NULL) {
        
        char* block_buffer = malloc(BLOCK_SIZE);
        u32   block_pointer;
        
        read_block(block_buffer, node.indirect / BLOCK_SIZE, BLOCK_SIZE);
        
        //scan the indirect block
        for(u32 i = 0; i < BLOCK_SIZE; i += sizeof(u32)) {
            
            block_pointer = *(u32*)&block_buffer[i];
            
            //deallocate the block
            if(block_pointer != SFS_NULL) {
                
                SET_BIT(g_free_block_bitmap[(block_pointer / BLOCK_SIZE) / 8], (block_pointer / BLOCK_SIZE) % 8, 0);
            }
        }
        
        SET_BIT(g_free_block_bitmap[(node.indirect / BLOCK_SIZE) / 8], (node.indirect / BLOCK_SIZE) % 8, 0);
        
        free(block_buffer);
        
    }

    char delet_this = 0;

    fwrite((char*)&delet_this, sizeof(char), 1, sfs.disk);
}

//returns file size
u32  sfs_file_size (sfs_file* file) {
    return file->node.size;
}

//set file data pointer
void sfs_file_seek (sfs_file* file, u32 offset) {
    file->data_pointer = offset;
}

//read file data pointer
u32 sfs_file_tell (sfs_file* file) {
    return file->data_pointer;
}























