//
// Created by curt white on 2020-03-11.
//
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "system.h"
#include "../disk/disk.h"

#define curr_block(size) (size / BLOCK_SIZE)
#define bytes_left(size) size - (size / BLOCK_SIZE) * BLOCK_SIZE

// Static Block Locations
const uint32_t SUPER_BLOCK_LOC = 0;
const uint32_t FREE_BLOCK_LOC = 1;
const uint32_t INODE_MAP_LOC = 2;
const uint32_t ROOT_DIR_LOC = 32;

// 64 per block @ 4 bytes per entry
const int INODE_MAP_SIZE = 2;
const int RESERVED_BLOCKS = 33;
const int INIT_BUFFER_SIZE = 10;
const int REFS_PER_INDIRECT = BLOCK_SIZE / 4;

#define MAX_INODES 256
#define MAX_FILE_SIZE 8459264

static uint32_t inode_map[MAX_INODES];
static unsigned char free_block_map[BLOCK_SIZE];

typedef struct super_block {
    uint32_t magic_number;
    uint32_t max_blocks;
    uint32_t root_dir_block;
    uint32_t max_inodes;
    uint32_t used_inodes; // Unused, can be found by searching the map
} super_block;

void llfs_print_inode(llfs_inode inode) {
    printf("Printing Inode \n");
    printf("File Size: %i\n", inode.file_size);
    printf("Flags: %i\n", inode.flags.type);

    printf("Direct: ");
    for (int i = 0; i < 10; i++) printf("%i ", inode.direct[i]);
    printf("\n");
    printf("Single Indirect: %i\n", inode.indirect);
    printf("Double Indirect: %i\n", inode.double_indirect);
}

void llfs_print_file(llfs_file *file) {
    printf("\n");
    if (file->loc_pointer != NULL)
        printf("Pointer Location: %i\n", file->loc_pointer[0]);
    printf("Byte Location: %i\n", file->pointer_byte_loc);
    printf("Inode Location: %i\n", file->inode_loc);
    llfs_print_inode(file->inode);

    int i = 0;
    if (file->ind.content != NULL) {
        printf("Single Indirect Blocks: ");
        while (file->ind.content[i] != 0 && i < REFS_PER_INDIRECT) {
            printf("%i ", file->ind.content[i]);
            i++;
        }
    }

    printf("\n");
    i = 0;
    if (file->dind.content != NULL) {
        printf("Double Indirect Blocks: ");
        while (file->dind.content[i] != 0 && i < REFS_PER_INDIRECT) {
            printf("%i ", file->dind.content[i]);
            i++;
        }
        printf("\n");
        printf("Single in Double: ");
        for (int j = 0; j < i; j ++) {
            int k = 0;
            while (file->dind.blocks[j].content[k] != 0 && k < REFS_PER_INDIRECT) {
                printf("%i ", file->dind.blocks[j].content[k]);
                k++;
            }
            printf("\n\n");
        }
        printf("\n");
    }
}

/**
 * Write a copy of the blocks data to the write buffer
 * @param buffer - The buffer to add the data to
 * @param block - A block of data to be copied to the write buffer
 * @return LLFS error message or 0 for success
 */
llfs_error write_buffer_cpy(llfs_write_buffer *buffer, file_block block) {
    file_block block_copy;
    block_copy.block_num = block.block_num;
    block_copy.t = FB_OWNED;

    char *data_copy = (char *) calloc(BLOCK_SIZE, sizeof(char));
    if (data_copy == NULL) return MEMORY_ALLOC_ERROR;
    block_copy.block_data = data_copy;

    memcpy(data_copy, block.block_data, BLOCK_SIZE);
    llfs_error e = write_buffer_append(buffer, block_copy);

    return e;
}

/**
 * Append a block item to the write buffer.
 * @param buffer - A buffer to append the item to
 * @param block - The file block to add to the buffer
 * @return LLFS error message
 */
llfs_error write_buffer_append(llfs_write_buffer *buffer, file_block block) {
    if (buffer->blocks == NULL || buffer->num_blocks == 0) {
        buffer->blocks = (file_block *) calloc(INIT_BUFFER_SIZE, sizeof(file_block));
        if (buffer->blocks == NULL) return MEMORY_ALLOC_ERROR;
        buffer->num_blocks = 0;
    } else if (buffer->num_blocks % INIT_BUFFER_SIZE == 0) {
        const int new_size = buffer->num_blocks + INIT_BUFFER_SIZE;
        buffer->blocks = (file_block *) realloc(buffer->blocks, sizeof(file_block) * new_size);
        if (buffer->blocks == NULL) return MEMORY_ALLOC_ERROR;
    }

    for (int i = 0; i < buffer->num_blocks; i++) {
        if (buffer->blocks[i].block_num == block.block_num) {
            return BUFFER_DUPLICATE_ERROR;
        }
    }

    buffer->blocks[buffer->num_blocks] = block;
    buffer->num_blocks ++;
    return 0;
}

llfs_error write_buffer_destroy(llfs_write_buffer *buffer) {
    for (int i = 0; i < buffer->num_blocks; i++) {
        if (buffer->blocks[i].t == FB_OWNED) {
            free(buffer->blocks[i].block_data);
        }
    }

    if ( buffer->blocks != NULL ) free(buffer->blocks);
    return 0;
}

/**
 * Reserve blocks and mark them on the block_map and return the blocks in the block_nums array
 * @param block_nums - Numbers of blocks which were reserved
 * @param block_count - The number of blocks which were reserved
 * @param block_map  - A map of blocks which are available
 * @return LLFS error message
 */
llfs_error llfs_reserve_blocks(int *block_nums, int block_count, unsigned char *block_map, int map_size) {
    unsigned char *map_copy = calloc(map_size, sizeof(char));
    if (map_copy == NULL) return MEMORY_ALLOC_ERROR;
    memcpy(map_copy, block_map, sizeof(char) * map_size);

    int byte = 0;
    int found = 0;
    for (int i = 0; i < block_count; i++) {
        while (map_copy[byte] == 0x00 && byte < map_size) byte++;

        unsigned char curr_byte = map_copy[byte];
        for (int j = 0; j < 8; j++) {
            if (curr_byte & 1u) {
                block_nums[i] = (byte * 8) + j;
                map_copy[byte] ^= (1u << (unsigned char)j);
                found ++;
                break;
            }

            curr_byte = curr_byte >> 1u;
        }
        // If disk is full do not return map_copy keep original map
        if (byte == map_size - 1 && map_copy[byte] == 0 && found != block_count) {
            free(map_copy);
            return DISK_FULL_ERROR;
        }
    }

    memcpy(block_map, map_copy, sizeof(char) * map_size);
    free(map_copy);

    return 0;
}

void print_bin(unsigned int n) {
    while (n) {
        if (n & 1u)
            printf("1");
        else
            printf("0");

        n >>= 1u;
    }
    printf("\n");
}

/**
 * Free blocks from the block map
 * @param block_num - The number of the block to free
 * @param block_map - The map to free the block from
 * @param map_size - The size of the map
 * @return llfs_error or 0 for success
 */
llfs_error free_blocks(int block_num, unsigned char *block_map, int map_size) {
    int index = block_num / 8;
    unsigned int bit_index = block_num % 8;

    if (index >= map_size) return BYTE_OUT_OF_RANGE_ERROR;
    block_map[index] |= (1u << (unsigned char) bit_index);

    return 0;
}

/**
 * Free an inode from the inode map
 * @param inode_num - The number of the inode to free
 * @param imap - The map to free the inode from
 * @param map_size - The size of the map to free the inode from
 * @return llfs_error or 0 for success
 */
llfs_error llfs_free_inode(uint32_t inode_num, uint32_t *imap, int map_size) {
    // No Inode 0 (starts at 1) cannot free the first inode
    if (inode_num == 0 || inode_num - 1 <= 0 ||  inode_num - 1 >= map_size) {
        return INODE_FREE_ERROR;
    }

    imap[inode_num - 1] = 0;
    return 0;
}

llfs_error llfs_reserve_inode(uint32_t block_num, uint32_t *imap, int map_size, int *imap_block, int *inode_num) {
    for (int i = 0; i < map_size; i++) {
        if (imap[i] == 0) {
            imap[i] = block_num;
            *inode_num = i + 1;
            *imap_block = (int)(i / (BLOCK_SIZE / sizeof(uint32_t)));
            return 0;
        }
    }

    return DISK_FULL_ERROR;
}

/**
 * Split a path into the directory and the name of the file
 * @param path - The whole path
 * @param file - An area to store the file name
 * @param dir_path - An area to store the path to file without the name
 * @return - llfs_error
 */
llfs_error llfs_get_file(char *path, char *file, char *dir_path) {
    int subdir_count = 0;
    char *pos = path;

    for (int i = 0; i < strlen(path); i++) {
        if (pos[i] == '/') {
            if (i == strlen(path) - 1) return BAD_PATH_ERROR;
            subdir_count ++;
            strncpy(dir_path, path, i);
            dir_path[i + 1] = '\0';

            unsigned int len = strlen(path) - i;
            strncpy(file, path + i + 1, len);
            file[len + 1] = '\0';
        }
    }

    if (strlen(file) > 31) return BAD_PATH_ERROR;
    if (subdir_count == 1) {
        dir_path[0] = '/';
        dir_path[1] = '\0';
    } else if (subdir_count == 0) {
        return BAD_PATH_ERROR;
    }

    return 0;
}

/**
 * Set the block at the position provided by p with the value of block
 * @param f - The file to set the block in
 * @param p - An indexing struct location where to set block
 * @param block - The block getting set
 * @return llfs_error or 0 for success
 */
llfs_error llfs_set_block(llfs_file *f, block_pos p, file_block block) {
    file_block *loc;
    switch (p.t) {
        case DIRECT:
            loc = &f->direct[p.l1];
            break;
        case IND:
            loc = &f->ind.blocks[p.l1];
            break;
        case DIND:
            loc = &f->dind.blocks[p.l1].blocks[p.l2];
            break;
        default:
            return INVALID_OPTION_ERROR;
    }

    // If we replace the data make sure we free it first
    if (loc->block_data != NULL) {
        free(loc->block_data);
    }

    *loc = block;
    return 0;
}

/**
 * Open a single block of data at store it at the location of pointer
 * @param f - The file to open the data into
 * @param p - The block position to put the piece of data
 * @param block_num - A block number for where the data is being stored on disk
 * @param pointer - A pointer to the data to store
 * @return llfs_error or 0 for success
 */
llfs_error llfs_open_block(llfs_file *f, block_pos p, int block_num, char **pointer) {
    char *block = (char *) calloc(BLOCK_SIZE, sizeof(char));
    if (block == NULL) return MEMORY_ALLOC_ERROR;

    disk_error e = disk_read_block(block_num, block);
    if (e != 0) {
        free(block);
        return DISK_ERROR;
    }

    file_block fb = { block_num, block, FB_OWNED };
    llfs_error err = llfs_set_block(f, p, fb);
    if (err != 0) {
        free(block);
        return DISK_ERROR;
    }

    *pointer = block;
    return 0;
}

/**
 * Open a single indirect block
 * @param ind - An indirect struct to store the data in
 * @param ind_loc - The location of that indirect block on disk (block number)
 * @param all_blocks - A buffer of all of the blocks being allocated so they can be freed if fails to open
 * @param curr_block - The current block number from the start of the file being opened
 * @return llfs_error or 0 for success
 */
llfs_error llfs_open_indirect(indirect *ind, int ind_loc, char **all_blocks, int *curr_block) {
    uint32_t *indirect = (uint32_t *) calloc(BLOCK_SIZE, sizeof(char *));
    if (indirect == NULL) return MEMORY_ALLOC_ERROR;

    file_block *fbs = (file_block *) calloc(BLOCK_SIZE, sizeof(char *));
    if (fbs == NULL) {
        free(indirect);
        return MEMORY_ALLOC_ERROR;
    }

    ind->blocks = fbs;
    ind->content = indirect;
    disk_error e = disk_read_block(ind_loc, (char *) indirect);
    if (e != 0) { free(indirect); free(fbs); return DISK_ERROR; }

    for (int i = 0; i < REFS_PER_INDIRECT; i++) {
        int block_num = indirect[i];
        if (block_num == 0) return 0;

        char *block = (char *) calloc(BLOCK_SIZE, sizeof(char *));
        if (block == NULL) { free(indirect); free(fbs); return MEMORY_ALLOC_ERROR; }

        disk_error err = disk_read_block(block_num, block);
        if (err != 0) { free(indirect); free(fbs); return DISK_ERROR; }

        all_blocks[*curr_block] = block;

        file_block fb = { block_num, block, FB_OWNED };
        fbs[i] = fb;

        (*curr_block)++;
    }

    return 0;
}

/**
 * Free all double indirect blocks
 * @param dind - A double indirect block struct
 * @param dind_loc - The block location of the double indirect block map
 * @param all_blocks - A buffer of all of the blocks being allocated so they can be freed if fails to open
 * @param curr_block - The current block number from the start of the file being opened
 * @return llfs_error or 0 for success
 */
llfs_error llfs_open_dind(double_indirect *dind, int dind_loc, char **all_blocks, int *curr_block) {
    uint32_t *double_indirect = (uint32_t *) calloc(BLOCK_SIZE, sizeof(char *));
    if (double_indirect == NULL) return MEMORY_ALLOC_ERROR;

    indirect *indirects = (indirect *) calloc(BLOCK_SIZE, sizeof(char *));
    if (indirects == NULL) {
        free(double_indirect);
        return MEMORY_ALLOC_ERROR;
    }

    dind->blocks = indirects;
    dind->content = double_indirect;
    disk_error e = disk_read_block(dind_loc, (char *) double_indirect);
    if (e != 0) {
        free(indirects);
        free(double_indirect);
        return DISK_ERROR;
    }

    int i = 0;
    llfs_error err = 0;
    while (i < REFS_PER_INDIRECT) {
        if (double_indirect[i] == 0) break;
        err = llfs_open_indirect(&indirects[i], double_indirect[i], all_blocks, curr_block);
        if (err != 0) break;
        i++;
    }

    if (err != 0) {
        free(indirects);
        free(double_indirect);
        return err;
    }

    return  0;
}

/**
 * Retrieve all of the file contents from disk and bring it into memory
 * @param inode - The inode to open the file from
 * @param file - The file to open the data into
 * @param inode_loc - The location of the inode block on disk
 * @return llfs_error or 0 for success
 */
llfs_error llfs_open_file(llfs_inode *inode, llfs_file *file, int inode_loc) {
    unsigned int total_blocks = ceil(((double) inode->file_size) / BLOCK_SIZE);
    if (inode->flags.type == DIR) { total_blocks = inode->flags.dir_blocks; }

    char **all_blocks = (char **) calloc(total_blocks, sizeof(char *));
    if (all_blocks == NULL) return MEMORY_ALLOC_ERROR;

    llfs_file new = { 0, 0, inode_loc, 0, *inode };
    memcpy(file, &new, sizeof(llfs_file));
    if (inode->file_size == 0 && total_blocks == 0) return EMPTY_FILE_ERROR;

    int curr_block = 0;
    block_pos p;
    llfs_error e = 0;
    while (curr_block < total_blocks) {
        if (curr_block < 10) {
            e = llfs_get_pos(file->pointer_byte_loc, &p);
            if (e != 0) break;
            e = llfs_open_block(file, p, inode->direct[curr_block], &all_blocks[curr_block]);
            if (e != 0) break;
            curr_block ++;
            file->pointer_byte_loc += BLOCK_SIZE;
        } else if (curr_block < REFS_PER_INDIRECT + 10) {
            e = llfs_open_indirect(&file->ind, file->inode.indirect, all_blocks, &curr_block);
            if (e != 0) break;
        } else {
            e = llfs_open_dind(&file->dind, file->inode.double_indirect, all_blocks, &curr_block);
            if (e != 0) break;
        }
    }

    // If opening any block fails all of the blocks that were opened are deallocated
    if (e != 0) {
        for (int i = 0; i < curr_block; ++i) {
            free(all_blocks[i]);
        }

        return e;
    }

    free(all_blocks);
    e = llfs_seek(file, LLFS_SEEK_START, 0);
    return e;
}

/**
 * Get the index of the requested byte in the file with index into the
 * @param byte - A number of the byte to move to
 * @param p - A struct holding the proper indexes to the file
 * @return llfs_error or 0 for success
 */
llfs_error llfs_get_pos(int byte, block_pos *p) {
    const int block = curr_block(byte);
    const int byte_loc = bytes_left(byte);

    if (byte < 0) return BYTE_OUT_OF_RANGE_ERROR;
    if (block < 10) {
        block_pos dir = { DIRECT, block, byte_loc, 0 };
        *p = dir;
    } else if (block < REFS_PER_INDIRECT + 10) {
        block_pos ind = { IND, block - 10, byte_loc, 0 };
        *p = ind;
    } else if (block < REFS_PER_INDIRECT * REFS_PER_INDIRECT + 10) {
        const int dind_loc = (block - 10 - REFS_PER_INDIRECT) / REFS_PER_INDIRECT;
        const int ind_loc = (block - 10 - REFS_PER_INDIRECT) - dind_loc * REFS_PER_INDIRECT;
        block_pos dind = { DIND, dind_loc, ind_loc, byte_loc };
        *p = dind;
    } else {
        return BYTE_OUT_OF_RANGE_ERROR;
    }

    p->byte = byte;
    return 0;
}

/**
 * Set the block pointer to a specific location
 * @param f - The file to set the pointer location in
 * @param p - The position where the block pointer will be set
 * @return llfs_error or 0 for success
 */
llfs_error llfs_set(llfs_file *f, block_pos p) {
    switch (p.t) {
        case DIRECT:
            f->loc_pointer = f->direct[p.l1].block_data + p.l2;
            break;
        case IND:
            f->loc_pointer = f->ind.blocks[p.l1].block_data + p.l2;
            break;
        case DIND:
            f->loc_pointer = f->dind.blocks[p.l1].blocks[p.l2].block_data + p.l3;
            break;
    }

    f->pointer_byte_loc = p.byte;
    return 0;
}

/**
 * Fetch the block at a given byte number
 * @param f - The file to get the block from
 * @param byte - the byte number where we will retrieve the file
 * @param block - The block
 * @return llfs_error or 0 for success
 */
llfs_error llfs_get_block(llfs_file *f, int byte, file_block *block) {
    block_pos p;
    llfs_error e = llfs_get_pos(byte, &p);
    if (e != 0) return e;

    switch (p.t) {
        case DIRECT:
            *block = f->direct[p.l1];
            break;
        case IND:
            *block = f->ind.blocks[p.l1];
            break;
        case DIND:
            *block = f->dind.blocks[p.l1].blocks[p.l2];
            break;
        default:
            return INVALID_OPTION_ERROR;
    }

    return 0;
}

/**
 * Seek a position in the file
 * @param f - The file to seek a position in
 * @param p - The type of seek (SET, START, END)
 * @param offset - The offset in bytes for SET type
 * @return llfs_error or 0 for success
 */
llfs_error llfs_seek(llfs_file *f, llfs_seek_pos p, int offset) {
    block_pos pos;

    if (p == LLFS_SEEK_START) {
        f->loc_pointer = f->direct[0].block_data;
        f->pointer_byte_loc = 0;
    } else if (p == LLFS_SEEK_END) {
        unwrap(llfs_get_pos(f->inode.file_size, &pos));
        unwrap(llfs_set(f, pos));
    } else if (p == LLFS_SEEK_SET) {
        unwrap(llfs_get_pos(offset, &pos));
        unwrap(llfs_set(f, pos));
    } else {
        return INVALID_OPTION_ERROR;
    }

    return 0;
}

llfs_error llfs_get_bytes(llfs_file *f, char *buffer, int num_bytes, int opt) {
    int curr_byte = 0;

    for (int i = 0; i < num_bytes; i++) {
        if (f->pointer_byte_loc == f->inode.file_size && opt != 1) return END_OF_FILE_ERROR;
        if (f->pointer_byte_loc % BLOCK_SIZE == 0) {
            unwrap(llfs_seek(f, LLFS_SEEK_SET, f->pointer_byte_loc));
        }

        buffer[curr_byte] = *f->loc_pointer;
        f->loc_pointer += 1;
        f->pointer_byte_loc += 1;
        curr_byte ++;
    }

    return 0;
}

llfs_error llfs_free_file_blocks(llfs_file *f) {
    int freed = 0;
    int total_blocks = ceil((double) f->inode.file_size / BLOCK_SIZE);
    if (f->inode_loc != 0) {
        free_blocks(f->inode_loc, free_block_map, BLOCK_SIZE);
    }

    for (int i = 0; i < 10 && freed < total_blocks; i++, freed++) {
        if (f->inode.direct[i] == 0) printf("We have an error direct %i\n", freed);
        free_blocks(f->inode.direct[i], free_block_map, BLOCK_SIZE);
    }

    if (total_blocks >= 10) {
        for (int i = 0; i < REFS_PER_INDIRECT; i++, freed++) {
            if (freed == total_blocks) break;
            int next = f->ind.content[freed - 10];
            if (next == 0) printf("We have an error indirect: %i\n", freed);
            free_blocks(next, free_block_map, BLOCK_SIZE);
        }

        if (f->inode.indirect == 0) printf("We have an error indirect: %i\n", freed);
        free_blocks(f->inode.indirect, free_block_map, BLOCK_SIZE);
    }

    if (total_blocks >= REFS_PER_INDIRECT + 10) {
        for (int j = 0; j < REFS_PER_INDIRECT; j++) {
            for (int i = 0; i < REFS_PER_INDIRECT; i++, freed++) {
                if (freed == total_blocks) break;
                int next = f->dind.blocks[j].content[i];
                if (next == 0) printf("We have an error dind %i\n", freed);
                free_blocks(next, free_block_map, BLOCK_SIZE);
            }

            if (f->dind.content[j] == 0) printf("We have an error nested ind: %i\n", freed);
            free_blocks(f->dind.content[j], free_block_map, BLOCK_SIZE);
            if (freed == total_blocks) break;
        }

        if (f->inode.double_indirect == 0) printf("We have an error double indirect: %i\n", freed);
        free_blocks(f->inode.double_indirect, free_block_map, BLOCK_SIZE);
    }

    return 0;
}

llfs_error llfs_dir_remove(llfs_write_buffer *w, llfs_file *f, char *file, int *inode_num) {
    dir_entry *buffer = (dir_entry *) calloc(BLOCK_SIZE, sizeof(char));
    if (buffer == NULL) return  MEMORY_ALLOC_ERROR;

    int seen = 0;
    int block_num = 0;
    llfs_error e = 0;
    while (seen < f->inode.file_size) {
        file_block fb;
        llfs_get_block(f, f->pointer_byte_loc, &fb);
        block_num = fb.block_num;

        e = llfs_get_bytes(f, (char *) buffer, BLOCK_SIZE, 1);
        if (e != 0 && e != END_OF_FILE_ERROR) return e;

        for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++) {
            if (buffer[i].inode != 0) {
                if (strcmp(file, buffer[i].name) == 0) {
                    *inode_num = buffer[i].inode;
                    buffer[i].inode = 0;
                    f->inode.file_size -= sizeof(dir_entry);
                    memcpy(fb.block_data, buffer, BLOCK_SIZE);
                    goto break_loop;
                }

                seen += sizeof(dir_entry);
                if (seen == f->inode.file_size) break;
            }
        }
    }

    break_loop:
    e = write_buffer_any(w, buffer, BLOCK_SIZE, block_num);
    free(buffer);
    return e;
}

/**
 * Free all of the blocks associated with a file and if using recursive the functions
 * will call itself on all sub directories and free all their blocks as well.
 * @param path - Path to the original file
 * @param recursive - 0 for non recursive, 1 for recursive
 * @param inode_num - Number of the inode so it can be freed from map
 * @return
 */
llfs_error llfs_free_files(char *path, int recursive, int inode_num) {
    int inode_loc;
    llfs_inode inode;
    llfs_file file;
    llfs_error e = llfs_get_inode(path, &inode, &inode_loc);
    if (e != 0) return e;

    e = llfs_open_file(&inode, &file, inode_loc);
    if (e != 0 && e != EMPTY_FILE_ERROR) { return e; }

    if (inode.flags.type == DIR && inode.file_size > 0) {
        if (!recursive) return NON_RECURSIVE_DELETE_ERROR;
        dir_entry *buffer = (dir_entry *) calloc(BLOCK_SIZE, sizeof(char));
        if (buffer == NULL) return  MEMORY_ALLOC_ERROR;
        int seen = 0;

        while (seen < inode.file_size) {
            e = llfs_get_bytes(&file, (char *) buffer, BLOCK_SIZE, 1);
            if (e != 0 && e != END_OF_FILE_ERROR) { free(buffer); return e; }

            for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++) {
                if (buffer[i].inode != 0) {
                    int new_path_len = strlen(path) + 33;
                    char *new_path = (char *) calloc(new_path_len * sizeof(char), sizeof(char));
                    if (new_path == NULL) { free(buffer); return MEMORY_ALLOC_ERROR;}
                    strncpy(new_path, path, strlen(path) + 1);
                    strncat(new_path, "/", 2);
                    strncat(new_path, buffer[i].name, 31);

                    e = llfs_free_files(new_path, 1, buffer[i].inode);
                    free(new_path);
                    if (e != 0) { free(buffer); return e;}
                    seen += sizeof(dir_entry);
                }
            }
        }

        free(buffer);
    }

    e = llfs_free_file_blocks(&file);
    if (e != 0) return e;
    if (inode_num != -1) {
        llfs_free_inode(inode_num, inode_map, MAX_INODES);
    }

    e = llfs_destroy_file(&file);
    return e;
}

llfs_error llfs_delete(char *path, int recursive) {
    char *dir_path = calloc((strlen(path) + 1), sizeof(char));
    char *file_name = calloc((strlen(path) + 1), sizeof(char));
    llfs_write_buffer w = { NULL, 0 };

    llfs_error e = llfs_get_file(path, file_name, dir_path);
    if (e != 0) { free(dir_path); free(file_name); return e; }

    int inode_loc;
    llfs_inode dir_inode;
    llfs_file file;

    e = llfs_get_inode(dir_path, &dir_inode, &inode_loc);
    if (e != 0) { free(dir_path); free(file_name); return e; }

    e = llfs_open_file(&dir_inode, &file, inode_loc);
    if (e != 0 && e != EMPTY_FILE_ERROR) { free(dir_path); free(file_name); return e; }

    e = llfs_free_files(path, recursive, -1);
    if (e != 0) { free(dir_path); free(file_name); return e; }

    int inode_num = 0;
    e = llfs_dir_remove(&w, &file, file_name, &inode_num);
    if (e != 0) { free(dir_path); free(file_name); return e; }

    e = llfs_free_inode(inode_num, inode_map, MAX_INODES);
    if (e != 0) { free(dir_path); free(file_name); return e; }

    e = write_buffer_any(&w, &file.inode, sizeof(llfs_inode), file.inode_loc);
    if (e != 0) goto free_exit;

    e = write_buffer_any(&w, (void *) free_block_map, BLOCK_SIZE, FREE_BLOCK_LOC);
    if (e != 0) goto free_exit;

    e = write_buffer_any(&w, inode_map, sizeof(uint32_t) * 128, INODE_MAP_LOC);
    if (e != 0) goto free_exit;

    e = write_buffer_any(&w, inode_map + 128, sizeof(uint32_t) * 128, INODE_MAP_LOC + 1);
    if (e != 0) goto free_exit;

    journal_new_transaction(w.blocks, w.num_blocks);
    free_exit:
    write_buffer_destroy(&w);
    llfs_destroy_file(&file);
    return e;
}

llfs_error llfs_write_bytes(llfs_write_buffer *w, llfs_file *f, const char *content, int num_bytes) {
    int curr_block = curr_block(f->pointer_byte_loc);
    int curr_byte = 0;

    for (int i = 0; i < num_bytes; i++) {
        if (f->inode.file_size == MAX_FILE_SIZE) return FILE_FULL_ERROR;
        if (f->inode.file_size % BLOCK_SIZE == 0 && f->pointer_byte_loc == f->inode.file_size) {
            int block_num;
            unwrap(llfs_extend_file(f, w, 1, &block_num));

            char *buffer = (char *) calloc(BLOCK_SIZE, sizeof(char));
            if (buffer == NULL) return MEMORY_ALLOC_ERROR;

            block_pos p;
            file_block fb = { block_num, buffer, FB_OWNED };

            llfs_error e = llfs_get_pos(f->inode.file_size, &p);
            if (e != 0) { free(buffer); return e; }

            e = llfs_set_block(f, p, fb);
            if (e != 0) { free(buffer); return e; }
        }

        if (f->pointer_byte_loc % BLOCK_SIZE == 0) {
            unwrap(llfs_seek(f, LLFS_SEEK_SET, f->pointer_byte_loc));
        }

        if (f->pointer_byte_loc == f->inode.file_size) {
            f->inode.file_size ++;
        }

        *f->loc_pointer = content[curr_byte];
        curr_byte ++;
        f->loc_pointer ++;
        f->pointer_byte_loc ++;
        int next_block = curr_block(f->pointer_byte_loc);
        if (next_block != curr_block) {
            file_block edited;
            unwrap(llfs_get_block(f, f->pointer_byte_loc - 1, &edited));
            unwrap(write_buffer_cpy(w, edited));
        } else if (curr_byte >= num_bytes) {
            file_block edited;
            unwrap(llfs_get_block(f, f->pointer_byte_loc, &edited));
            unwrap(write_buffer_cpy(w, edited));
        }

        curr_block = next_block;
    }

    return 0;
}

llfs_error llfs_write(char *content, int size, int count, llfs_file *file) {
    llfs_write_buffer w = { NULL, 0 };
    const int total_size = size * count;

    llfs_error e = llfs_write_bytes(&w, file, content, total_size);
    if (e != 0) return e;

    e = write_buffer_any(&w, &file->inode, sizeof(llfs_inode), file->inode_loc);
    if (e != 0) { write_buffer_destroy(&w); return e; }

    e = write_buffer_any(&w, (void *) free_block_map, BLOCK_SIZE, FREE_BLOCK_LOC);
    if (e != 0) { write_buffer_destroy(&w); return e; }

    journal_new_transaction(w.blocks, w.num_blocks);
    write_buffer_destroy(&w);
    return 0;
}

/**
 * Append a file to the directory
 * @param w - write buffer to append events to
 * @param f - A file to write the data to
 * @param dir - The directory entry to add to the file
 * @return llfs_error
 */
llfs_error llfs_dir_append(llfs_write_buffer *w, llfs_file *f, dir_entry dir) {
    unwrap(llfs_seek(f, LLFS_SEEK_START, 0));
    dir_entry *block = (dir_entry *) calloc (BLOCK_SIZE, sizeof(char));
    if (block == NULL) return MEMORY_ALLOC_ERROR;

    int block_num = 0;
    llfs_error e = 0;
    if (f->inode.file_size == BLOCK_SIZE * f->inode.flags.dir_blocks) {
        e = llfs_extend_file(f, w, 1, &block_num);
        if (e != 0) { free(block); return e; }
        f->inode.flags.dir_blocks += 1;
        memcpy(block, &dir, sizeof(dir_entry));
    } else {
        while (1) {
            file_block fb;
            e = llfs_get_block(f, f->pointer_byte_loc, &fb);
            if (e != 0) { free(block); return e; }
            block_num = fb.block_num;

            llfs_error e = llfs_get_bytes(f, (char *) block, BLOCK_SIZE, 0);
            if (e != 0 && e != END_OF_FILE_ERROR) return e;

            for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++) {
                if (block[i].inode == 0) {
                    block[i] = dir;
                    goto break_loop;
                }
            }

            if (f->pointer_byte_loc == f->inode.file_size) return FILE_FULL_ERROR;
        }
        break_loop: ;
    }

    file_block fb = { block_num, (char *) block, FB_OWNED };
    block_pos p;

    f->inode.file_size += sizeof(dir_entry);
    e = write_buffer_any(w, block, BLOCK_SIZE, block_num);
    if (e != 0 && e != BUFFER_DUPLICATE_ERROR) { free(block); return e; }
    e = llfs_get_pos(curr_block(f->inode.file_size), &p);
    if (e != 0) { free(block); return e; }
    e = llfs_set_block(f, p, fb);
    if (e != 0) { free(block); return e; }

    return 0;
}

/**
 * Write any data to the buffer
 * @param w - A write buffer object
 * @param content - The content to be stored in the buffer
 * @param size - The size of the content in bytes. Should be a maximum of BLOCK_SIZE
 * @param block - Block number which the data will be written to
 * @return llfs_error
 */
llfs_error write_buffer_any(llfs_write_buffer *w, void *content, int size, int block) {
    char *buffer = (char *) calloc(BLOCK_SIZE, sizeof(char));
    if (buffer == NULL) return MEMORY_ALLOC_ERROR;

    memcpy(buffer, content, size);
    file_block f = { block, buffer, FB_OWNED };
    write_buffer_append(w, f);

    return 0;
}

llfs_error llfs_search_dir(llfs_file *f, char *next_level, int *inode_block) {
    unwrap(llfs_seek(f, LLFS_SEEK_START, 0));

    dir_entry *buffer = (dir_entry *) calloc(BLOCK_SIZE, sizeof(char));
    if (buffer == NULL) return MEMORY_ALLOC_ERROR;

    int total_blocks = ceil(((double) f->inode.file_size) / BLOCK_SIZE);
    int inode_num = 0;
    llfs_error e = 0;
    for (int i = 0; i < total_blocks; i++) {
        e = llfs_get_bytes(f, (char *) buffer, BLOCK_SIZE, 1);
        if (e != 0 && e != END_OF_FILE_ERROR) break;
        else e = 0;

        for (int j = 0; j < BLOCK_SIZE / sizeof(dir_entry); j++) {
            if (strcmp(buffer[j].name, next_level) == 0 && buffer[j].inode != 0) {
                inode_num = buffer[j].inode;
                goto break_loop;
            }
        }
    }
    break_loop:;
    if (inode_num != 0) {
        *inode_block = inode_map[inode_num - 1];
    } else {
        e = FILE_NOT_FOUND_ERROR;
    }

    free(buffer);
    return e;
}

/**
 * Create a new file.
 * @param path - The path to the new file including the files name
 * @param t - The type of the new file. Either flat or dir
 * @return llfs_error
 */
llfs_error llfs_create_file(char *path, file_type t) {
    char *dir_path = calloc((strlen(path) + 1), sizeof(char));
    char *file_name = calloc((strlen(path) + 1), sizeof(char));
    llfs_write_buffer w = { NULL, 0 };
    llfs_inode node = { 0, { t, 0 }, { 0 }, 0, 0 };
    llfs_error e = llfs_get_file(path, file_name, dir_path);
    if (e != 0) goto free_exit;

    int inode_loc;
    llfs_inode dir_inode;
    llfs_file file;
    e = llfs_get_inode(dir_path, &dir_inode, &inode_loc);
    if (e == FILE_NOT_FOUND_ERROR) {
        e = BAD_PATH_ERROR;
        goto free_exit;
    }
    if (e != 0 && e != EMPTY_FILE_ERROR) goto free_exit;

    e = llfs_open_file(&dir_inode, &file, inode_loc);
    if (e != 0 && e != EMPTY_FILE_ERROR) goto free_exit;

    int test_block = 0;
    e = llfs_search_dir(&file, file_name, &test_block);
    if (e != 0 && e != FILE_NOT_FOUND_ERROR) goto free_exit;
    if (e != FILE_NOT_FOUND_ERROR) {
        e = FILE_ALREADY_EXISTS_ERROR;
        goto free_exit;
    }

    int inode_block = 0, inode_num = 0, map_block = 0;
    e = llfs_reserve_blocks(&inode_block, 1, free_block_map, BLOCK_SIZE);
    if (e != 0) goto free_exit;

    e = llfs_reserve_inode(inode_block, inode_map, MAX_INODES, &map_block, &inode_num);
    if (e != 0) goto free_exit;

    dir_entry d = { inode_num };
    strncpy(d.name, file_name, 31);
    e = llfs_dir_append(&w, &file, d);
    if (e != 0) goto free_exit;

    e = write_buffer_any(&w, &file.inode, sizeof(llfs_inode), inode_loc);
    if (e != 0) goto free_exit;

    e = write_buffer_any(&w, &node, sizeof(llfs_inode), inode_block);
    if (e != 0) goto free_exit;

    e = write_buffer_any(&w, inode_map + (128 * map_block), sizeof(uint32_t) * 128, INODE_MAP_LOC + map_block);
    if (e != 0) goto free_exit;

    e = write_buffer_any(&w, (void *) free_block_map, BLOCK_SIZE, FREE_BLOCK_LOC);
    if (e != 0) goto free_exit;

    e = journal_new_transaction(w.blocks, w.num_blocks);

    /**
     * I know people hate goto but I do not understand why if the code is jumping to a very clear
     * location like this people still disapprove.
     */
    free_exit:
    write_buffer_destroy(&w);
    free(dir_path);
    free(file_name);

    return e;
}

llfs_error llfs_destroy_file(llfs_file *file) {
    unwrap(llfs_seek(file, LLFS_SEEK_START, 0));

    int loc = file->pointer_byte_loc;
    while (loc < file->inode.file_size) {
        free(file->loc_pointer);
        loc += BLOCK_SIZE;
        if (loc >= file->inode.file_size) break;
        unwrap(llfs_seek(file, LLFS_SEEK_SET, loc));
    }

    if (file->inode.file_size > 10 * BLOCK_SIZE) {
        free(file->ind.content);
        free(file->ind.blocks);
    }

    int blocks_left = curr_block(file->inode.file_size) - 138;
    if (blocks_left == 0) return 0;

    int double_indirect = ceil((double) blocks_left / REFS_PER_INDIRECT);
    for (int i = 0; i < double_indirect; i++) {
        free(file->dind.blocks[i].blocks);
    }

    if (double_indirect > 0) {
        free(file->dind.blocks);
    }

    return 0;
}

/**
 * Creating a double indirect and add it directly to the file
 * @param file - The file to add the double indirect to
 * @param w - A write buffer to add any updates to
 * @return - llfs_error
 */
llfs_error llfs_add_double_indirect(llfs_file *file, llfs_write_buffer *w) {
    int loc = 0;
    unwrap(llfs_reserve_blocks(&loc, 1, free_block_map, BLOCK_SIZE));

    uint32_t *content = (uint32_t *) calloc(BLOCK_SIZE, sizeof(char));
    if (content == NULL) return MEMORY_ALLOC_ERROR;

    indirect *singles = (indirect *) calloc(REFS_PER_INDIRECT, sizeof(indirect));
    if (singles == NULL) { free(singles); return MEMORY_ALLOC_ERROR; }

    content[0] = loc;
    double_indirect d = { content, singles };

    file->dind = d;
    file->inode.double_indirect = loc;
    file_block f = { loc, (char *) content, FB_REF };

    llfs_error e = write_buffer_append(w, f);
    if (e != 0 && e != BUFFER_DUPLICATE_ERROR) {
        free(content);
        free(singles);
        return e;
    }

    return 0;
}

/**
 * Create a single indirect block in the location denoted by opt and pos. Though this function updates
 * the Inode, it does not add it to the buffer in case there is also a double indirect being added.
 * @param file - The file to add the new indirect block to
 * @param w - A write buffer to add any changes to
 * @param opt - If 0 then add to immediate indirect, if 1 add to the double indirect list, else error
 * @param pos - The position in the double indirect if applicable
 * @return - llfs_error, 0 if successful
 */
llfs_error llfs_add_indirect(llfs_file *file, llfs_write_buffer *w, int opt, int pos) {
    int loc = 0;
    unwrap(llfs_reserve_blocks(&loc, 1, free_block_map, BLOCK_SIZE));

    uint32_t *content = (uint32_t *) calloc(BLOCK_SIZE, sizeof(char));
    if (content == NULL) return MEMORY_ALLOC_ERROR;

    file_block *ind_blocks = (file_block *) calloc(REFS_PER_INDIRECT, sizeof(file_block));
    if (ind_blocks == NULL) { free(content); return MEMORY_ALLOC_ERROR;}

    indirect i = { content, ind_blocks };
    file_block f = { loc, (char *) content, FB_REF };

    llfs_error e = 0;
    if (opt == 0) { // Add indirect
        file->ind = i;
        file->inode.indirect = loc;
        e = write_buffer_append(w, f);
    } else if (opt == 1) { // Add indirect to double
        if (file->dind.blocks == NULL){
            e = llfs_add_double_indirect(file, w);
            if (e != 0 && e != BUFFER_DUPLICATE_ERROR) {
                free(content);
                free(ind_blocks);
                return e;
            }
        }

        file->dind.content[pos] = (uint32_t) loc;
        file->dind.blocks[pos] = i;

        e = write_buffer_append(w, f);
        if (e != 0 && e != BUFFER_DUPLICATE_ERROR) { free(content); free(ind_blocks); return e; }

        file_block dind = { file->inode.double_indirect, (char *) file->dind.content, FB_REF };
        e = write_buffer_append(w, dind);
    } else {
        return INVALID_OPTION_ERROR;
    }

    if (e != 0 && e != BUFFER_DUPLICATE_ERROR) {
        free(content);
        free(ind_blocks);
    }

    return e;
}

/**
 * Extend a file by adding new block. Will also commit indirect blocks to the buffer as well.
 * @param file - File to extend
 * @param w - A buffer to add block updates to
 * @param num_blocks - The number of blocks to reserve
 * @param blocks - An array to store the block numbers in
 * @return llfs_error or 0 for success.
 */
llfs_error llfs_extend_file(llfs_file *file, llfs_write_buffer *w, int num_blocks, int *blocks) {
    llfs_error e = llfs_reserve_blocks(blocks, num_blocks, free_block_map, BLOCK_SIZE);
    if (e != 0) return e;

    const int pnum = REFS_PER_INDIRECT;
    int next_loc = curr_block(file->inode.file_size);
    for (int i = 0; i < num_blocks; i ++) {

        if (next_loc < 10) { // Direct
            file->inode.direct[next_loc] = blocks[i];

        } else if (next_loc == 10) { // First Indirect
            e = llfs_add_indirect(file, w, 0, 0);
            if (e != 0 && e != BUFFER_DUPLICATE_ERROR) return e;
            file->ind.content[0] = (uint32_t) blocks[i];

        } else if (next_loc < pnum + 10) { // On Single Indirect
            const int ind_loc = next_loc - 10;
            file->ind.content[ind_loc] = (uint32_t) blocks[i];

            file_block f = {file->inode.indirect, (char *) file->ind.content, FB_REF};
            e = write_buffer_append(w, f);
            if (e != 0 && e != BUFFER_DUPLICATE_ERROR) return e;

        } else if (next_loc == pnum + 10) { // First Double Indirect
            e = llfs_add_indirect(file, w, 1, 0);
            if (e != 0 && e != BUFFER_DUPLICATE_ERROR) return e;
            file->dind.blocks[0].content[0] = (uint32_t) blocks[i];

        } else if (next_loc > (pnum + 10) && (next_loc - 10) % pnum == 0) { // New Indirect in Double Indirect
            const int dind_loc = (next_loc - 10 - pnum) / pnum;
            e = llfs_add_indirect(file, w, 1, dind_loc);
            if (e != 0 && e != BUFFER_DUPLICATE_ERROR) return e;
            file->dind.blocks[dind_loc].content[0] = (uint32_t) blocks[i];

        } else { // In Indirect in Double Indirect
            const int dind_loc = (next_loc - 10 - pnum) / pnum;
            const int ind_loc = (next_loc - 10 - pnum) - dind_loc * pnum;
            file->dind.blocks[dind_loc].content[ind_loc] = (uint32_t) blocks[i];

            file_block f = {file->dind.content[dind_loc], (char *) file->dind.blocks[dind_loc].content, FB_REF};
            e = write_buffer_append(w, f);
            if (e != 0 && e != BUFFER_DUPLICATE_ERROR) return e;
        }

        next_loc ++;
    }

    return 0;
}

/**
 * Load the inode from the block location into the pointer
 * @param inode - Load inode to this location
 * @param inode_loc - Block number of inode
 * @return llfs_error or 0 for success.
 */
llfs_error llfs_open_inode(llfs_inode *inode, int inode_loc) {
    char *inode_buffer = (char *) calloc(BLOCK_SIZE, sizeof(char));
    if (inode_buffer == NULL) return MEMORY_ALLOC_ERROR;

    disk_error e = disk_read_block(inode_loc, inode_buffer);
    if (e != 0) {
        free(inode_buffer);
        return DISK_ERROR;
    }

    memcpy(inode, inode_buffer, sizeof(llfs_inode));
    free(inode_buffer);
    return 0;
}

int count_levels(char *path) {
    int count = 0;
    for (int i = 0; i < strlen(path); ++i) {
        if (path[i] == '/') count++;
    }

    return count;
}

/**
 * Fetch the inode from disk. Traverse the directory tree until found.
 * @param path - The path to the inode to search for
 * @param inode - The inode pointer to load the data into
 * @param inode_loc - The block location of the inode as return value
 * @return llfs_error or 0 for success. Error if file not found
 */
llfs_error llfs_get_inode(char *path, llfs_inode *inode, int *inode_loc) {
    *inode_loc = ROOT_DIR_LOC;
    unwrap(llfs_open_inode(inode, *inode_loc));
    if (inode->file_size == 0) return EMPTY_FILE_ERROR;

    char *path_cpy = (char *) calloc(strlen(path) + 1, sizeof(char));
    if (path_cpy == NULL) return MEMORY_ALLOC_ERROR;
    strncpy(path_cpy, path, strlen(path) + 1);

    llfs_file f;
    llfs_error e = 0;
    char *tok = strtok(path_cpy, "/");
    int levels = count_levels(path);
    int iter = 0;
    while (tok != NULL) {
        e = llfs_open_file(inode, &f, *inode_loc);
        if (iter != levels && e == EMPTY_FILE_ERROR) {
            e = FILE_NOT_FOUND_ERROR;
        }
        if (e != 0) break;

        e = llfs_search_dir(&f, tok, inode_loc);
        if (e != 0) break;

        tok = strtok(NULL, "/");
        llfs_open_inode(inode, *inode_loc);

        llfs_destroy_file(&f);
        iter++;
    }

    free(path_cpy);
    return e;
}

/**
 * Format the root directory
 * @return llfs_error or 0 for success
 */
llfs_error create_root() {
    char *buffer = (char *) calloc(BLOCK_SIZE, sizeof(char));
    if (buffer == NULL) return MEMORY_ALLOC_ERROR;

    llfs_inode node = { 0, { DIR, 0 }, { 0 }, 0, 0 };
    memcpy(buffer, &node, sizeof(llfs_inode));

    disk_error de = disk_write_block(ROOT_DIR_LOC, buffer);
    if (de != 0) return DISK_ERROR;

    free(buffer);
    return 0;
}

/**
 * Configure the IMAP
 * @return llfs_error or 0 on success
 */
llfs_error inode_map_config() {
    char *buffer = (char *) calloc(BLOCK_SIZE, sizeof(char));
    if (buffer == NULL) return MEMORY_ALLOC_ERROR;

    for (int i = 1; i < INODE_MAP_SIZE; i ++) {
        disk_error de = disk_write_block(INODE_MAP_LOC + i, buffer);
        if (de != 0) return DISK_ERROR;
    }

    buffer[0] = ROOT_DIR_LOC;
    disk_error de = disk_write_block(INODE_MAP_LOC, buffer);
    if (de != 0) return DISK_ERROR;

    inode_map[0] = ROOT_DIR_LOC;

    free(buffer);
    return 0;
}

/**
 * Load the data from the disk into memory
 * @return llfs_error or 0 on success
 */
llfs_error llfs_load() {
    disk_error de = disk_read_block(FREE_BLOCK_LOC, (char *) free_block_map);
    if (de != 0) return DISK_ERROR;

    de = disk_read_block(INODE_MAP_LOC, (char *) inode_map);
    if (de != 0) return DISK_ERROR;
    de = disk_read_block(INODE_MAP_LOC + 1u, (char *)&inode_map[MAX_INODES / INODE_MAP_SIZE]);
    if (de != 0) return DISK_ERROR;

    return journal_recover();
}

/**
 * Initialize the file system. This is equivalent to formatting a disk, It also loads
 * important data into memory upon initialization.
 * @return llfs_error or 0 on success
 */
llfs_error llfs_init() {
    char *buffer = (char *) calloc(BLOCK_SIZE, sizeof(char));
    if (buffer == NULL) return MEMORY_ALLOC_ERROR;

    // Super block Config
    super_block s = { 0x0000, BLOCK_COUNT, ROOT_DIR_LOC, MAX_INODES, 1 };
    memcpy(buffer, &s, sizeof(super_block));
    disk_error de = disk_write_block(SUPER_BLOCK_LOC, buffer);
    if (de != 0) return DISK_ERROR;

    // Free block bitmap
    memset(free_block_map, 0xFF, BLOCK_SIZE);
    int blocks[RESERVED_BLOCKS];
    llfs_error e = llfs_reserve_blocks(blocks, RESERVED_BLOCKS, free_block_map, BLOCK_SIZE);
    if (e != 0) { free(buffer); return e;}

    de = disk_write_block(FREE_BLOCK_LOC, (char *) free_block_map);
    if (de != 0) return DISK_ERROR;

    unwrap(journal_init());
    unwrap(inode_map_config());
    unwrap(create_root());

    free(buffer);
    return 0;
}
