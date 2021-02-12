//
// Created by curt white on 2020-03-09.
//

#ifndef DISK_INCLUDED
#define DISK_INCLUDED

#define DISK_SIZE 2*1024*1024
#define BLOCK_SIZE 512
#define BLOCK_COUNT 4096

typedef enum disk_error {
    BLOCK_OUT_OF_BOUNDS = 1,
    DISK_ALREADY_LOADED,
    DISK_NOT_LOADED,
    DISK_LOAD_FAILED,
    DISK_WRITE_ERROR,
    DISK_SEEK_ERROR
} disk_error;

const char *disk_strerror(disk_error e);

int disk_is_mounted();
disk_error disk_unmount();

disk_error disk_read_block(int block_num, char *block);
disk_error disk_write_block(int block_num, char *block);
disk_error disk_mount(char *disk_name);

#endif