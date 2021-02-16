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

#ifndef CSC369_E2FS_H
#define CSC369_E2FS_H

#include "ext2.h"
#include "ext2fsal.h"
#include <string.h>
#include <time.h>


// Returns the inode number of the entry that matches the name inside the inode directory given. 0 is returned otherwise.
unsigned int in_dir(unsigned int inode, char *name);

// Returns the inode that belongs at the end of the path.
// Returns 0 if invalid path
unsigned int get_inode(char *path);

// Returns the path to the parent of the file in the given path.
// Will always return. You have to use get_inode to check for validity
char *path_to_parent(char *path);

// Returns a new dir entry within the inode given. If no space available, return NULL. The rec_len
// will be automatically set by this helper. This helper takes block allocation and indirect blocks in consideration.
struct ext2_dir_entry *_create_dir(unsigned int inode, int name_len);

// Creates a new entry with the given inode and name within 
int create_dir(unsigned int parent_inode, unsigned int file_inode, int file_type, char *file_name);

// Find the name of the child *FILE* (the last entry) in a given path
char *child_name(char *path);

// Finds the first free inode in the inode bitmap, and returns the corresponding inode
// Note: this return the inode number, NOT the inode index within the inode bitmap
unsigned int set_free_inode();

// Checks if the given inode is set in the inode bitmap or not. Returns 1 if set and 0 if isn't
int inode_is_set(unsigned int inode);

// Returns the block number of the first free block, zero otherwise
unsigned int set_free_block();

// Clears the given block number in the block bitmap
void clear_block(unsigned int bnum);

// Clears the corresponding inode from the inode bitmap
void clear_inode(unsigned int inode);

#endif