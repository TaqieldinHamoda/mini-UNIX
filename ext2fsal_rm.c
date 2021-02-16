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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


int32_t ext2_fsal_rm(const char *path)
{
    /**
     * implement the ext2_rm command here ...
     * the argument path is the path to the file to be removed.
     */

    char *parent = path_to_parent((char *) path);
    char *dir_name = child_name((char *) path);
    unsigned int inode = get_inode(parent);

    if(inode == 0){
        return ENONET;
    }

    pthread_mutex_lock(&inodes_lock[inode - 1]);
    unsigned int del_inode = in_dir(inode, dir_name);

    if((dir_name[0] == '/') || (del_inode == 0)){
        pthread_mutex_unlock(&inodes_lock[inode - 1]);
        return ENONET;
    }

    pthread_mutex_lock(&inodes_lock[del_inode - 1]);

    if(EXT2_S_IFDIR & inodes[del_inode - 1].i_mode){
        pthread_mutex_unlock(&inodes_lock[del_inode - 1]);
        pthread_mutex_unlock(&inodes_lock[inode - 1]);
        return EISDIR;
    }

    int byte_index;
    int prev_byte_index = 0;
    unsigned char *block;
    struct ext2_dir_entry *dir;
    struct ext2_dir_entry *prev_dir;

    // Check if the name provided matches any files or directories within the given directory
    for(int j = 0; j < inodes[inode - 1].i_blocks/2; j++){
        byte_index = 0;
        block = (unsigned char *)(disk + 1024 * (inodes[inode - 1].i_block[j]));

        while(byte_index < EXT2_BLOCK_SIZE){
            dir = (struct ext2_dir_entry *)(block + byte_index);

            if(dir->inode == del_inode){
                break;
            }

            prev_byte_index = byte_index;
            byte_index += dir->rec_len;
        }
    }

    inodes[inode - 1].i_size -= sizeof(struct ext2_dir_entry) + dir->name_len;

    if(byte_index > 0){
        prev_dir = (struct ext2_dir_entry *)(block + prev_byte_index);
        prev_dir->rec_len += dir->rec_len;
    }

    dir->inode = 0;
    dir->file_type = EXT2_FT_UNKNOWN;

    inodes[del_inode - 1].i_links_count--;
    // Reset the blocks allocated to the inode
    if(inodes[del_inode - 1].i_links_count == 0){
        for(int k = 0; k < inodes[del_inode - 1].i_blocks/2; k++){
            if(k <= 12){
                clear_block(inodes[del_inode - 1].i_block[k]);
            }else{
                unsigned int *indirect = (unsigned int *)(disk + 1024 * (inodes[del_inode - 1].i_block[12]));
                clear_block(indirect[k - 13]);
            }
        }

        // Reset the inode
        clear_inode(del_inode);

        inodes[del_inode - 1].i_size = 0;
        inodes[del_inode - 1].i_blocks = 0;
        inodes[del_inode - 1].i_links_count = 0;
        inodes[del_inode - 1].i_mode = 0x0000;
        inodes[del_inode - 1].i_dtime = (unsigned int) time(NULL);
    }
    pthread_mutex_unlock(&inodes_lock[del_inode - 1]);
    pthread_mutex_unlock(&inodes_lock[inode - 1]);

    return 0;
}