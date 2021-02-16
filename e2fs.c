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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "e2fs.h"


unsigned int in_dir(unsigned int inode, char *name){
    int byte_index = 0;
    unsigned char *block;
    struct ext2_dir_entry *dir;

    // If the inode belongs to a file then return
    if(!(EXT2_S_IFDIR & inodes[inode - 1].i_mode)){ return 0; }

    // Check if the name provided matches any files or directories within the given directory
    for(int j = 0; j < inodes[inode - 1].i_blocks/2; j++){
        byte_index = 0;
        block = (unsigned char *)(disk + 1024 * (inodes[inode - 1].i_block[j]));

        while(byte_index < EXT2_BLOCK_SIZE){
            dir = (struct ext2_dir_entry *)(block + byte_index);

            if(strncmp(dir->name, name, dir->name_len) == 0){
                return dir->inode;
            }

            byte_index += dir->rec_len;
        }
    }

    return 0;
}


unsigned int get_inode(char *path){
    if(path[0] != '/'){ return 0; } // If the path given is not absolute, then no bueno

    char curr_dir[EXT2_NAME_LEN];
    int curr_inode = EXT2_ROOT_INO;

    curr_dir[0] = '\0';

    // Traverse the whole path given. If no errors, then return the inode pointed to by the end of the path
    // If a part of the path is invalid, return -1
    // /level1/link1
    for(int i = 1; i < strlen(path); i++){
        if((i == strlen(path) - 1) && (path[i] != '/')){
            strncat(curr_dir, &path[i], 1);
        }else if(path[i] != '/'){
            strncat(curr_dir, &path[i], 1);
            continue;
        }else if(strlen(curr_dir) == 0){
            continue;
        }

        if((curr_inode = in_dir(curr_inode, curr_dir)) == 0){
            return 0;
        }

        curr_dir[0] = '\0';
    }

    return curr_inode;
}

char *path_to_parent(char *path){
    char *parent = malloc(sizeof(char) * strlen(path));
    int slash = 0;
    int last_slash = 0;

    parent[0] = '\0';
    strncat(parent, path, 1);

    // Copy the path over with no redudant slashes in the path.
    for(int i = 1; i < strlen(path); i++){
        if(path[i] != '/'){
            strncat(parent, &path[i], 1);
            last_slash = slash;
            continue;
        }else if(path[i - 1] == '/'){
            continue;
        }

        strcat(parent, "/");
        slash = strlen(parent) - 1;
    }

    if(last_slash != 0){
        parent[last_slash] = '\0';
    }else{ parent[1] = '\0'; }
    
    return parent;
}


struct ext2_dir_entry *_create_dir(unsigned int inode, int name_len){
    unsigned short byte_index = 0;
    unsigned char *block;
    struct ext2_dir_entry *dir = NULL;

    unsigned int bnum = inodes[inode - 1].i_blocks/2;

    if(inodes[inode - 1].i_blocks == 0){ // No blocks have been allocated to the inode
        bnum = set_free_block();        
        inodes[inode - 1].i_blocks += 2;
        inodes[inode - 1].i_block[inodes[inode - 1].i_blocks/2 - 1] = bnum;
        block = (unsigned char *)(disk + 1024 * (bnum));

        dir = (struct ext2_dir_entry *)(block + byte_index);
        dir->rec_len = EXT2_BLOCK_SIZE;
    }else if(bnum < 13){
        block = (unsigned char *)(disk + 1024 * (inodes[inode - 1].i_block[bnum - 1]));

        // Traverse throught the blocks in the given inode until space is found
        while(byte_index < EXT2_BLOCK_SIZE){
            dir = (struct ext2_dir_entry *)(block + byte_index);
            byte_index += dir->rec_len;
        }

        // Space available in the last allocated direct block
        unsigned short size = sizeof(struct ext2_dir_entry) + dir->name_len;
        if(dir->rec_len - size - (4 - size%4) > sizeof(struct ext2_dir_entry) + name_len){
            byte_index -= dir->rec_len;
            dir->rec_len = size;
            dir->rec_len = dir->rec_len + (4 - dir->rec_len%4);
            byte_index += dir->rec_len;
        }else{ // Allocate a new direct block
            bnum = set_free_block();
            inodes[inode - 1].i_blocks += 2;
            inodes[inode - 1].i_block[inodes[inode - 1].i_blocks/2 - 1] = bnum;
            block = (unsigned char *)(disk + 1024 * (bnum));
            byte_index = 0;
        }

        dir = (struct ext2_dir_entry *)(block + byte_index);
        dir->rec_len = EXT2_BLOCK_SIZE - byte_index;
        inodes[inode - 1].i_size = EXT2_BLOCK_SIZE*(inodes[inode - 1].i_blocks/2);
    }

    return dir;
}


int create_dir(unsigned int parent_inode, unsigned int file_inode, int file_type, char *file_name){
    struct ext2_dir_entry *dir = _create_dir(parent_inode, strlen(file_name));

    // If space is available, then create the desired file. Else, return -1
    if(dir){
        dir->inode = file_inode;
        dir->file_type = file_type;
        dir->name_len = strlen(file_name);
        strncpy(dir->name, file_name, dir->name_len);
        inodes[file_inode - 1].i_links_count++;

        // Set the proper file type
        if(file_type == EXT2_FT_REG_FILE){
            inodes[file_inode - 1].i_mode = EXT2_S_IFREG;
        }else if(file_type == EXT2_FT_SYMLINK){
            inodes[file_inode - 1].i_mode = EXT2_S_IFLNK;
        }else if(file_type == EXT2_FT_DIR){
            inodes[file_inode - 1].i_mode = EXT2_S_IFDIR;
        }

        return 0;
    }

    return -1;
}

/**
 * Find the name of the child in the given path
 * 
 * Examples:
 * child_name(/level1) -> level1
 * child_name(/level1/) -> level1
 * child_name(/level1/level2/) -> level2
 * child_name(/level1/level2) -> level2
 * chile_name(/) -> /
 * 
 * Returns:
 * Success: A string of the child name
 * Failure: NULL
 */
char *child_name(char *path){
    char p[strlen(path) + 1];
    strncpy(p, path, strlen(path) + 1);

    char *child_n = malloc(sizeof(char) * (EXT2_NAME_LEN + 1));
    child_n[0] = '\0';

    char *curr_entry_name = strtok(p, "/");
    if(curr_entry_name == NULL){ // case: / or /////////
        strcat(child_n, "/");
        return child_n;
    }

    while (curr_entry_name != NULL){
        strcpy(child_n, curr_entry_name);
        curr_entry_name = strtok(NULL, "/");
    }
    return child_n;
}

/**
 * Find's the first free inode, and sets it to 1 in the bitmap
 * 
 * Returns:
 * Success: The corressponding Inode, NOT the inode index.
 * Failure: -1, i.e no free inode found
 */
unsigned int set_free_inode(){
    pthread_mutex_lock(&sb_lock);
    if(sb->s_free_inodes_count == 0){
        pthread_mutex_unlock(&sb_lock);
        return 0;
    }

    unsigned int inode_idx = 0; // indexing starts at 0, but we start numbering inodes from 1
    pthread_mutex_lock(&inode_bits_lock);
    for(int i = 0; i < sb->s_inodes_count/8; i++){
        for(int j = 0; j < 8; j++){
            if(!(inode_bits[i] & (1 << j))){
                inode_bits[i] |= 1 << j; // set the bit to 1
                pthread_mutex_unlock(&inode_bits_lock);

                sb->s_free_inodes_count--;
                pthread_mutex_unlock(&sb_lock);

                pthread_mutex_lock(&inodes_lock[inode_idx]);
                inodes[inode_idx].i_size = 0;
                inodes[inode_idx].i_blocks = 0;
                inodes[inode_idx].i_links_count = 0;
                inodes[inode_idx].i_mode = 0x0000; // Set to unknown type initially
                inodes[inode_idx].i_dtime = 0;
                inodes[inode_idx].osd1 = 0;
                inodes[inode_idx].i_generation = 0;
                inodes[inode_idx].i_file_acl = 0;
                inodes[inode_idx].i_dir_acl = 0;
                inodes[inode_idx].i_faddr = 0;
                pthread_mutex_unlock(&inodes_lock[inode_idx]);

                pthread_mutex_lock(&gd_lock);
                gd->bg_free_inodes_count--;
                pthread_mutex_unlock(&gd_lock);

                return inode_idx + 1;
            }
            inode_idx++;
        }
    }

    pthread_mutex_unlock(&inode_bits_lock);
    pthread_mutex_unlock(&sb_lock);
    return 0;
}


unsigned int set_free_block(){
    pthread_mutex_lock(&sb_lock);
    if(sb->s_free_blocks_count == 0){
        pthread_mutex_unlock(&sb_lock);
        return 0;
    }

    unsigned int counter = 0;
    pthread_mutex_lock(&block_bits_lock);
    for(int i = 0; i < sb->s_blocks_count/8; i++){
        for(int j = 0; j < 8; j++){
            if(!(block_bits[i] & (1 << j))){
                block_bits[i] |= 1 << j;
                pthread_mutex_unlock(&block_bits_lock);

                sb->s_free_blocks_count--;
                pthread_mutex_unlock(&sb_lock);

                pthread_mutex_lock(&gd_lock);
                gd->bg_free_blocks_count--;
                pthread_mutex_unlock(&gd_lock);

                return counter + 1;
            }
            counter++;
        }
    }

    pthread_mutex_unlock(&block_bits_lock);
    pthread_mutex_unlock(&sb_lock);
    return 0;
}


void clear_block(unsigned int bnum){
    unsigned int counter = 0;

    pthread_mutex_lock(&sb_lock);
    for(int i = 0; i < sb->s_blocks_count/8; i++){
        for(int j = 0; j < 8; j++){
            if(counter == bnum - 1){
                sb->s_free_blocks_count++;
                pthread_mutex_unlock(&sb_lock);
                
                pthread_mutex_lock(&block_bits_lock);
                block_bits[i] &= ~(1 << j);
                pthread_mutex_unlock(&block_bits_lock);

                pthread_mutex_lock(&gd_lock);
                gd->bg_free_blocks_count++;
                pthread_mutex_unlock(&gd_lock);
                return;
            }
            counter++;
        }
    }
    pthread_mutex_unlock(&sb_lock);
}


void clear_inode(unsigned int inode){
    unsigned int counter = 0;
    pthread_mutex_lock(&sb_lock);
    for(int i = 0; i < sb->s_inodes_count/8; i++){
        for(int j = 0; j < 8; j++){
            if(counter == inode - 1){
                sb->s_free_inodes_count++;
                pthread_mutex_unlock(&sb_lock);

                pthread_mutex_lock(&inode_bits_lock);
                inode_bits[i] &= ~(1 << j); // set the bit to 1
                pthread_mutex_unlock(&inode_bits_lock);

                pthread_mutex_lock(&gd_lock);
                gd->bg_free_inodes_count++;
                pthread_mutex_unlock(&gd_lock);
                return;
            }
            counter++;
        }
    }
    pthread_mutex_unlock(&sb_lock);
}