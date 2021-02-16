/*
 *------------
 * This code is provided solely for the personal and private use of
 * students taking the CSC369H5 course at the University of Toronto.
 * Copying for purposes other than this use is expressly prohibited.
 * All forms of distribution of this code, whether as given or with
 * any changes, are expressly prohibited.
 *
 * All of the files in this directory and all subdirectories are:
 * Copyright (c) 2019 MCS @ UTM
 * -------------
 */

#include "ext2fsal.h"
#include "e2fs.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>



int32_t ext2_fsal_ln_sl(const char *src,
                        const char *dst)
{
    /**
     * TODO: implement the ext2_ln_sl command here ...
     * src and dst are the ln command arguments described in the handout.
     */

    unsigned int child_inode = get_inode((char *) dst);
    if(child_inode != 0){ // child exists
        pthread_mutex_lock(&inodes_lock[child_inode-1]);
        if(inodes[child_inode - 1].i_mode & EXT2_S_IFDIR){
            pthread_mutex_unlock(&inodes_lock[child_inode-1]);
            return EISDIR;
        }
        pthread_mutex_unlock(&inodes_lock[child_inode-1]);
        return EEXIST;
    }


    int blocks_required = 0;
    unsigned int file_size = strlen(src);
    if(file_size <= EXT2_BLOCK_SIZE){
        blocks_required = 1;
    }
    else{
        if(file_size % EXT2_BLOCK_SIZE == 0){
            blocks_required = (file_size / EXT2_BLOCK_SIZE);
        }
        else{
            blocks_required = (file_size / EXT2_BLOCK_SIZE) + 1;
        }
        
    }

    // child does not exist
    char *parent = path_to_parent((char *) dst);
    unsigned int parent_inode = get_inode(parent);
    if(parent_inode == 0){
        return ENOENT; // parent does not exist
    }
    pthread_mutex_lock(&inodes_lock[parent_inode - 1]);
    if (!(EXT2_S_IFDIR & inodes[parent_inode - 1].i_mode)){ // parent is NOT a directory
        pthread_mutex_unlock(&inodes_lock[parent_inode - 1]);
        return ENOSPC;
    }

    pthread_mutex_lock(&sb_lock);
    if (blocks_required > sb->s_free_blocks_count) // check how many blocks are required before setting a free inode
    {
        pthread_mutex_unlock(&sb_lock);
        pthread_mutex_unlock(&inodes_lock[parent_inode - 1]);
        return ENOSPC;
    }
    pthread_mutex_unlock(&sb_lock);

    unsigned int inode_num = set_free_inode();
    if(inode_num == 0){
        pthread_mutex_unlock(&inodes_lock[parent_inode - 1]);
        return ENOSPC; // not enough space
    }
    pthread_mutex_lock(&inodes_lock[inode_num - 1]);

    char *new_child_name = child_name((char *) dst);
    
    if(create_dir(parent_inode, inode_num, EXT2_FT_SYMLINK, new_child_name) == -1){
        pthread_mutex_unlock(&inodes_lock[inode_num - 1]);
        pthread_mutex_unlock(&inodes_lock[parent_inode - 1]);
        return ENOSPC; // not enough space
    }
    
    // store the path
    int bytes_written = 0;
    for(int i = 0; i < blocks_required; i++){
        unsigned int b = set_free_block();
        
        inodes[inode_num-1].i_block[i] = b;
        inodes[inode_num - 1].i_blocks += 2;
        
        unsigned char *block = (unsigned char *)(disk + 1024 * (b));
        for(int j = 0; j < EXT2_BLOCK_SIZE; j++){
            if(bytes_written >= file_size){
                break;
            }
            block[j] = src[bytes_written];
            bytes_written++;
        }
    }
    inodes[inode_num - 1].i_size = file_size;

    pthread_mutex_unlock(&inodes_lock[inode_num - 1]);
    pthread_mutex_unlock(&inodes_lock[parent_inode - 1]);
    return 0;
}
