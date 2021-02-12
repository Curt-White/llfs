//
// Created by curt white on 2020-03-09.
//
#include <stdio.h>
#include <string.h>

#include "disk.h"

static const char *DISK_ERROR_STRING[] = {
        "Operation Successfully Completed",
        "The Block Number Provided Is Out Of Bounds",
        "A Disk Has Already Been Loaded, Unmount First",
        "A Disk Must Be Loaded Before Attempting Operation",
        "Failed To Load Disk And One Could Not Be Created",
        "An Error Has Occurred While Writing To Disk",
        "An Error Has Occurred While Seeking On Disk"
};

FILE *disk;

const char *disk_strerror(disk_error e) {
    return DISK_ERROR_STRING[e];
}

/**
 * Check if there is a disk currently mounted
 * @return disk_error
 */
int disk_is_mounted() {
    return disk == NULL ? 0 : 1;
}

/**
 * Unmount the disk which is currently mounted
 * @return disk_error
 */
disk_error disk_unmount() {
    if (disk != NULL) {
        int res = fclose(disk);
        if (res != 0) return DISK_NOT_LOADED;

        disk = NULL;
        return 0;
    }

    return DISK_NOT_LOADED;
}

/**
 * Read a single block from the disk
 * @param block_num - The id of the block to read to
 * @param block - Buffer to store block data
 * @return 0 for success, otherwise error
 */
disk_error disk_read_block(int block_num, char *block) {
    if (disk == NULL) return DISK_NOT_LOADED;
    if (block_num >= BLOCK_COUNT || block_num < 0) return BLOCK_OUT_OF_BOUNDS;

    int res = fseek(disk, BLOCK_SIZE * block_num, SEEK_SET);
    if (res != 0) return DISK_SEEK_ERROR;

    res = fread(block, sizeof(char), BLOCK_SIZE, disk);
    if (res != BLOCK_SIZE) return DISK_WRITE_ERROR;

    return 0;
}

/**
 * Write a single block to the disk
 * @param block_num - The id of the block to write
 * @param block - Block data to be written
 * @return 0 for success, otherwise error
 */
disk_error disk_write_block(int block_num, char *block) {
    if (disk == NULL) {
        return DISK_NOT_LOADED;
    }

    if (block_num >= BLOCK_COUNT || block_num < 0) {
        return BLOCK_OUT_OF_BOUNDS;
    }

    int res = fseek(disk, BLOCK_SIZE * block_num, SEEK_SET);
    if (res != 0) return DISK_SEEK_ERROR;

    res = fwrite(block, sizeof(char), BLOCK_SIZE, disk);
    if (res != BLOCK_SIZE) return DISK_WRITE_ERROR;

    return 0;
}

disk_error disk_init(FILE *disk_file) {
    const char *buffer[BLOCK_SIZE] = { 0 };

    int res = fseek(disk_file, 0, SEEK_SET);
    if (res != 0) return DISK_SEEK_ERROR;

    for (int i = 0; i < BLOCK_COUNT; i ++) {
        res = fwrite(buffer, sizeof(char), BLOCK_SIZE, disk_file);
        if (res != BLOCK_SIZE) return DISK_WRITE_ERROR;
    }

    return 0;
}

/**
 * Mount a disk to use for reading and writing to
 * @param disk_name - Name of disk to be mounted
 * @return 0 for success, otherwise error
 */
disk_error disk_mount(char *disk_name) {
    if (disk != NULL) {
        return DISK_ALREADY_LOADED;
    }

    FILE *disk_file = fopen(disk_name, "rb+");
    if (disk_file == NULL) {
        disk_file = fopen(disk_name, "wb+x");

        if (disk_file == NULL) {
            return DISK_LOAD_FAILED;
        }

        int res = disk_init(disk_file);
        if (res != 0) return res;
    }

    disk = disk_file;
    return 0;
}
