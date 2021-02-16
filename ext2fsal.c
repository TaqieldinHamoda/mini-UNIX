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
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int fd;
extern unsigned char *disk;
extern struct ext2_super_block *sb;
extern struct ext2_group_desc *gd;
extern unsigned char *block_bits;
extern unsigned char *inode_bits;
extern struct ext2_inode *inodes;


void ext2_fsal_init(const char* image)
{
    /**
     * Initialization tasks, e.g., initialize synchronization primitives used,
     * or any other structures that may need to be initialized in your implementation,
     * open the disk image by mmap-ing it, etc.
     */
    fd = open(image, O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        return;
    }

    sb = (struct ext2_super_block *)(disk + 1024);
    gd = (struct ext2_group_desc *)(disk + 2048);
    block_bits = (unsigned char *)(disk + (gd->bg_block_bitmap * 1024));
    inode_bits = (unsigned char *)(disk + (gd->bg_inode_bitmap * 1024));
    inodes = (struct ext2_inode *)(disk + (gd->bg_inode_table * 1024));

    pthread_mutex_init(&sb_lock, NULL);
    pthread_mutex_init(&gd_lock, NULL); 
    pthread_mutex_init(&block_bits_lock, NULL); 
    pthread_mutex_init(&inode_bits_lock, NULL); 

    inodes_lock = malloc(sizeof(pthread_mutex_t) * sb->s_inodes_count);
    for(int i = 0; i < sb->s_inodes_count; i++){
        pthread_mutex_init(&inodes_lock[i], NULL); 
    }
}

void ext2_fsal_destroy()
{
    /**
     * Cleanup tasks, e.g., destroy synchronization primitives, munmap the image, etc.
     */
    for(int i = 0; i < sb->s_inodes_count; i++){
        pthread_mutex_destroy(&inodes_lock[i]); 
    }

    free(inodes_lock);
    pthread_mutex_destroy(&sb_lock);
    pthread_mutex_destroy(&gd_lock);
    pthread_mutex_destroy(&block_bits_lock);
    pthread_mutex_destroy(&inode_bits_lock);

    if(munmap(disk, 128 * 1024)){
        close(fd);
        perror("munmap");
        return;
    }

    close(fd);
}