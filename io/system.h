//
// Created by curt white on 2020-03-11.
//

#ifndef SYSTEM_INCLUDED
#define SYSTEM_INCLUDED

#include <stdint.h>
#include "error.h"
#include "journal.h"

typedef enum llfs_seek_pos {
    LLFS_SEEK_START,
    LLFS_SEEK_END,
    LLFS_SEEK_SET,
} llfs_seek_pos;

typedef enum pos_type {
    DIRECT,
    IND,
    DIND
} pos_type;

typedef struct block_pos {
    pos_type t;
    int l1;
    int l2;
    int l3;
    int byte;
} block_pos;

typedef enum file_type {
    FLAT,
    DIR
} file_type;

typedef struct dir_entry {
    uint8_t inode;
    char name[31]; // Including terminator
} dir_entry;

typedef struct llfs_inode {
    uint32_t file_size;
    struct {                    // Upgraded this to a bit field instead of int from spec
        unsigned int type : 3;
        unsigned int dir_blocks : 8;
        unsigned int reserved : 21;
    } flags;
    uint16_t direct[10];
    uint16_t indirect;
    uint16_t double_indirect;
} llfs_inode;

typedef struct indirect {
    uint32_t *content;
    file_block *blocks;
} indirect;

typedef struct double_indirect {
    uint32_t *content;
    indirect *blocks;
} double_indirect;

typedef struct llfs_file {
    char *loc_pointer;
    int pointer_byte_loc;
    int inode_loc;
    char *inode_content;
    llfs_inode inode;
    file_block direct[10];
    indirect ind;
    double_indirect dind;
} llfs_file;

typedef struct llfs_write_buffer {
    file_block *blocks;
    int num_blocks;
} llfs_write_buffer;

void llfs_print_file(llfs_file *file);
void llfs_print_inode(llfs_inode inode);

llfs_error llfs_load();
llfs_error llfs_init();
llfs_error llfs_free_file_blocks(llfs_file *f);
llfs_error llfs_delete(char *path, int recursive);
llfs_error llfs_get_pos(int byte, block_pos *p);
llfs_error free_blocks(int block_num, unsigned char *block_map, int map_size);
llfs_error llfs_write(char *content, int size, int count, llfs_file *file);
llfs_error llfs_open_file(llfs_inode *inode, llfs_file *file, int inode_loc);
llfs_error llfs_dir_append(llfs_write_buffer *w, llfs_file *f, dir_entry dir);
llfs_error llfs_get_bytes(llfs_file *f, char *buffer, int num_bytes, int opt);
llfs_error llfs_destroy_file(llfs_file *file);
llfs_error llfs_reserve_inode(uint32_t block_num, uint32_t *imap, int map_size, int *imap_block, int *inode_num);
llfs_error llfs_seek(llfs_file *f, llfs_seek_pos p, int offset);
llfs_error llfs_get_inode(char *path, llfs_inode *inode, int *inode_loc);
llfs_error llfs_create_file(char *path, file_type t);
llfs_error llfs_extend_file(llfs_file *file, llfs_write_buffer *w, int num_blocks, int *blocks);
llfs_error llfs_reserve_blocks(int *block_nums, int block_count, unsigned char *block_map, int map_size);

llfs_error write_buffer_destroy(llfs_write_buffer *buffer);
llfs_error write_buffer_any(llfs_write_buffer *w, void *content, int size, int block);
llfs_error write_buffer_append(llfs_write_buffer *buffer, file_block block);

#endif
