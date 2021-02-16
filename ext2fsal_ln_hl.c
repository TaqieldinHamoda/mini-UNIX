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


int32_t ext2_fsal_ln_hl(const char *src,
                        const char *dst)
{
    /**
     * implement the ext2_ln_hl command here ...
     * src and dst are the ln command arguments described in the handout.
     */

    char *dst_parent = path_to_parent((char *) dst);
    char *dst_file = child_name((char *) dst);
    unsigned int orig_inode = get_inode((char *) src);
    unsigned int dst_inode = get_inode(dst_parent);

    if((dst_inode == 0) || (orig_inode == 0) || (dst_inode == orig_inode)){
        return ENOENT;
    }

    pthread_mutex_lock(&inodes_lock[orig_inode - 1]);
    pthread_mutex_lock(&inodes_lock[dst_inode - 1]);

    // Error checking
    if(EXT2_S_IFDIR & inodes[orig_inode - 1].i_mode){
        pthread_mutex_unlock(&inodes_lock[dst_inode - 1]);
        pthread_mutex_unlock(&inodes_lock[orig_inode - 1]);
        return EISDIR;
    }else if(in_dir(dst_inode, dst_file) != 0){
        pthread_mutex_unlock(&inodes_lock[dst_inode - 1]);
        pthread_mutex_unlock(&inodes_lock[orig_inode - 1]);
        return EEXIST;
    }

    // Create the hard link
    if(create_dir(dst_inode, orig_inode, EXT2_FT_REG_FILE, dst_file) == -1){
        pthread_mutex_unlock(&inodes_lock[dst_inode - 1]);
        pthread_mutex_unlock(&inodes_lock[orig_inode - 1]);
        return ENOSPC;
    }

    pthread_mutex_unlock(&inodes_lock[dst_inode - 1]);
    pthread_mutex_unlock(&inodes_lock[orig_inode - 1]);
    return 0;
}
