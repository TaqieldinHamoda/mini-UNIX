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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int32_t ext2_fsal_cp(const char *src,
                     const char *dst)
{
    /**
     * TODO: implement the ext2_cp command here ...
     * src and dst are the ln command arguments described in the handout.
     */
    FILE *fp = fopen(src, "r"); // ALWAYS close!
    if (fp == NULL)
    {
        return ENOENT; // didn't find the file
    }

    // check if file already exist, if so we would be overwriting
    unsigned int inode_num = 0; // the desired inode we will be writing to
    char *dst_child_name = child_name((char *) dst);
    unsigned int child_inode = get_inode((char *)dst);

    if(child_inode != 0){
        pthread_mutex_lock(&inodes_lock[child_inode - 1]);
    }
    
    fseek(fp, 0, SEEK_END);
    unsigned int file_size = ftell(fp);
    int blocks_required = 0;
    if (file_size < EXT2_BLOCK_SIZE)
    {
        blocks_required = 1;
    }
    else
    {
        if(file_size % EXT2_BLOCK_SIZE == 0){
            blocks_required = (file_size / EXT2_BLOCK_SIZE);
        }
        else{
            blocks_required = (file_size / EXT2_BLOCK_SIZE) + 1;
        }
    }

    if (child_inode == 0)
    { // child doesn't exist
        char *parent = path_to_parent((char *)dst);
        unsigned int parent_inode = get_inode(parent);
        if (parent_inode == 0)
        {
            return ENOENT; // parent does not exist
        }
        pthread_mutex_lock(&inodes_lock[parent_inode - 1]);
        if (!(EXT2_S_IFDIR & inodes[parent_inode - 1].i_mode)){ // parent is NOT a directory
            pthread_mutex_unlock(&inodes_lock[parent_inode - 1]);
            fclose(fp);
            return ENOSPC;
        }

            pthread_mutex_lock(&sb_lock);
        if (blocks_required > sb->s_free_blocks_count) // check how many blocks are required before setting a free inode
        {
            pthread_mutex_unlock(&sb_lock);
            pthread_mutex_unlock(&inodes_lock[parent_inode - 1]);
            fclose(fp);
            return ENOSPC;
        }
        pthread_mutex_unlock(&sb_lock);

        inode_num = set_free_inode();
        if (inode_num == 0)
        {   
            pthread_mutex_unlock(&inodes_lock[parent_inode - 1]);
            fclose(fp);
            return ENOSPC; // not enough space
        }
        pthread_mutex_lock(&inodes_lock[inode_num - 1]);

        unsigned int create_check = create_dir(parent_inode, inode_num, EXT2_FT_REG_FILE, dst_child_name);
        if (create_check == -1)
        {   
            pthread_mutex_unlock(&inodes_lock[inode_num - 1]);
            pthread_mutex_unlock(&inodes_lock[parent_inode - 1]);
            fclose(fp);
            return ENOSPC; // not enough space
        }
        pthread_mutex_unlock(&inodes_lock[parent_inode - 1]);
        // inode_num = new_child_inode;
    }
    else
    { // child exists
        if (EXT2_S_IFDIR & inodes[child_inode - 1].i_mode)
        { // child is directory
            // create new file in said directory
            char *src_child = child_name((char *) src);
            if(dst[strlen(dst) -1 ] != '/'){
                strcat((char *) dst, "/");
            }
            strcat((char *) dst, src_child);
            inode_num = get_inode((char *) dst);
            if(inode_num == 0){ // src_child does not exist within directory
                pthread_mutex_lock(&sb_lock);
                if (blocks_required > sb->s_free_blocks_count) // check how many blocks are required before setting a free inode
                {
                    pthread_mutex_unlock(&sb_lock);
                    pthread_mutex_unlock(&inodes_lock[child_inode - 1]);
                    fclose(fp);
                    return ENOSPC;
                }
                pthread_mutex_unlock(&sb_lock);

                inode_num = set_free_inode();
                if (inode_num == 0)
                {
                    pthread_mutex_unlock(&inodes_lock[child_inode - 1]);
                    fclose(fp);
                    return ENOSPC; // not enough space
                }
                pthread_mutex_lock(&inodes_lock[inode_num - 1]);
                char *new_child_name = child_name((char *)src);
                unsigned int create_check = create_dir(child_inode, inode_num, EXT2_FT_REG_FILE, new_child_name);
                if (create_check == -1)
                {   
                    pthread_mutex_unlock(&inodes_lock[child_inode - 1]);
                    pthread_mutex_unlock(&inodes_lock[inode_num - 1]);
                    fclose(fp);
                    return ENOSPC; // not enough space
                }
                pthread_mutex_unlock(&inodes_lock[child_inode - 1]);
            }
            else{
                pthread_mutex_lock(&inodes_lock[inode_num - 1]);
                pthread_mutex_unlock(&inodes_lock[child_inode - 1]);
            }
            
            // inode_num = new_child_inode;
        }
        else
        {   
            inode_num = child_inode;
        }
    }

    int blocks_acquired = inodes[inode_num - 1].i_blocks / 2;
    int delete_flag = 0;

    int blocks_written_to = blocks_acquired;
    if(blocks_acquired > 12){
        blocks_written_to--; // neglects the indirect pointer itself
    }

    int extra_blocks_needed = blocks_required - blocks_written_to;

    if(extra_blocks_needed < 0){
        delete_flag = 1;
        if(blocks_required <= 12 && blocks_acquired > 12){
            extra_blocks_needed--; // want to delete indirect pointer itself
        }
    }

    if((blocks_acquired <= 12) && (delete_flag == 0)){ // NOT deleting
        if(extra_blocks_needed + blocks_acquired > 12){
            extra_blocks_needed++; // include indirect pointer itself
        }
    }
    pthread_mutex_lock(&sb_lock);
    if (extra_blocks_needed > (int) sb->s_free_blocks_count)
    {
        pthread_mutex_unlock(&sb_lock);
        pthread_mutex_unlock(&inodes_lock[inode_num - 1]);
        fclose(fp);
        return ENOSPC;
    }
    pthread_mutex_unlock(&sb_lock);

    if (extra_blocks_needed > 0)
    {
        int extra_idx = blocks_acquired; // starting place to append to i_block[]

        unsigned int b;
        int block_idx = 0; // starting index to append to indirect block, if necessary
        for (int i = 0; i < extra_blocks_needed; i++)
        {
            if (extra_idx > 12)
            {
                unsigned int *block = (unsigned int *)(disk + 1024 * (inodes[inode_num - 1].i_block[12]));
                b = set_free_block();
                block[block_idx] = b;
                inodes[inode_num - 1].i_blocks += 2;
                block_idx++;
            }
            else
            {
                b = set_free_block();
                inodes[inode_num - 1].i_block[extra_idx] = b;
                inodes[inode_num - 1].i_blocks += 2;
                extra_idx++;
            }
        }
    }
    else if(extra_blocks_needed < 0){ // need to deallocate blocks
        int need_to_delete = -1*extra_blocks_needed; 
        int start_delete = blocks_required; // start index to delete blocks

        int block_idx = 0;  // starting index to append to indirect block, if necessary
        unsigned int b; // block we will deallocate
        for(int i = 0; i < need_to_delete; i++){
            if(start_delete > 12){
                unsigned int *block = (unsigned int *)(disk + 1024 * (inodes[inode_num - 1].i_block[12]));
                b = block[block_idx];
                clear_block(b);
                inodes[inode_num - 1].i_blocks -= 2;
                block_idx++;
            }
            else{
                b = inodes[inode_num - 1].i_block[start_delete];
                clear_block(b);
                inodes[inode_num - 1].i_blocks -= 2;
                start_delete++;
            }
        }
    }

    // blocks are allocated, now let's write to them

    fseek(fp, 0, SEEK_SET); // bring file pointer back to beginning

    // add content to direct pointer
    for (int i = 0; i < inodes[inode_num - 1].i_blocks / 2; i++)
    {
        if (i == 12)
        { // we hit an indirect pointer
            break;
        }
        unsigned char *block = (unsigned char *)(disk + 1024 * (inodes[inode_num - 1].i_block[i]));

        // write file to blocks
        int c_val;
        unsigned char c;
        int block_idx = 0;
        while ((c_val = fgetc(fp)) != EOF && block_idx < EXT2_BLOCK_SIZE)
        {
            c = (unsigned char)c_val;
            block[block_idx] = c;
            block_idx++;
        }
    }

    // add content to indirect pointer, if necessary
    if (inodes[inode_num - 1].i_blocks / 2 > 12)
    {
        int counter = 0;
        unsigned int *block = (unsigned int *)(disk + 1024 * (inodes[inode_num - 1].i_block[12]));
        while (counter < (inodes[inode_num - 1].i_blocks / 2 - 13))
        {
            unsigned int bnum = block[counter];
            unsigned char *block_data = (unsigned char *)(disk + 1024 * (bnum));

            int c_val;
            unsigned char c;
            int block_idx = 0;
            while ((c_val = fgetc(fp)) != EOF && block_idx < EXT2_BLOCK_SIZE)
            {
                c = (unsigned char)c_val;
                block_data[block_idx] = c;
                block_idx++;
            }
            counter++;
        }
    }
    inodes[inode_num - 1].i_size = file_size;
    
    pthread_mutex_unlock(&inodes_lock[inode_num - 1]);
    fclose(fp);
    return 0;
}