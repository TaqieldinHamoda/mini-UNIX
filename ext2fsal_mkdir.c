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


int32_t ext2_fsal_mkdir(const char *path)
{
    /**
     * implement the ext2_mkdir command here ...
     * the argument path is the path to the directory to be created.
     */

    char *parent = path_to_parent((char *) path);
    char *dir_name = child_name((char *) path);
    unsigned int inode = get_inode(parent);

    if(inode == 0){
        return ENOENT;
    }

    pthread_mutex_lock(&inodes_lock[inode - 1]);

    // Error checking
    if(!(EXT2_S_IFDIR & inodes[inode - 1].i_mode)){
        pthread_mutex_unlock(&inodes_lock[inode - 1]);
        return ENOENT;
    }
    
    if(dir_name[0] == '/'){
        pthread_mutex_unlock(&inodes_lock[inode - 1]);
        return ENONET;
    }else if(in_dir(inode, dir_name) != 0){
        if(!(inodes[in_dir(inode, dir_name) - 1].i_mode & EXT2_S_IFDIR)){
            pthread_mutex_unlock(&inodes_lock[inode - 1]);
            return ENONET;
        }else{
            pthread_mutex_unlock(&inodes_lock[inode - 1]);
            return EEXIST;
        }
    }

    // Get the first free inode from the inode bitmap
    int free_inode = set_free_inode();
    if(free_inode == 0){
        pthread_mutex_unlock(&inodes_lock[inode - 1]);
        return ENOSPC;
    }

    pthread_mutex_lock(&inodes_lock[free_inode - 1]);

    // Create the directory within the path
    if(create_dir(inode, free_inode, EXT2_FT_DIR, dir_name) == -1){
        pthread_mutex_unlock(&inodes_lock[free_inode - 1]);
        pthread_mutex_unlock(&inodes_lock[inode - 1]);
        return ENOSPC;
    }

    // Create the ./ and ../ directories
    create_dir(free_inode, free_inode, EXT2_FT_DIR, ".");
    create_dir(free_inode, inode, EXT2_FT_DIR, "..");

    pthread_mutex_unlock(&inodes_lock[free_inode - 1]);
    pthread_mutex_unlock(&inodes_lock[inode - 1]);

    // Increment the directory count
    pthread_mutex_lock(&gd_lock);
    gd->bg_used_dirs_count++;
    pthread_mutex_unlock(&gd_lock);

    return 0;
}