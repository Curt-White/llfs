//
// Created by curt white on 2020-03-09.
//

#include <stdlib.h>
#include <stdio.h>
#include "system.h"
#include "File.h"

// This is small because the disk is very small and we need to ensure
// that the journal does not wrap around and write over itself
const int MAX_WRITE_SIZE = 2048;

llfs_error InitLLFS() {
    return llfs_init();
}

llfs_error llfs_fseek(llfs_file *file, llfs_seek_opt p, int offset) {
    return llfs_seek(file, (llfs_seek_pos) p, offset);
}

llfs_error llfs_fopen(char *path, llfs_file **file) {
    llfs_file *f = (llfs_file *) calloc(1, sizeof(llfs_file));
    if (f == NULL) return MEMORY_ALLOC_ERROR;

    int loc;
    llfs_inode i;
    llfs_error e = llfs_get_inode(path, &i, &loc);
    if (e != 0) {
        if (e == EMPTY_FILE_ERROR) e = FILE_NOT_FOUND_ERROR;
        *file = NULL;
        free(f);
        return e;
    }

    e = llfs_open_file(&i, f, loc);
    if (e == EMPTY_FILE_ERROR) e = 0;
    if (e != 0) {
        *file = NULL;
        free(f);
        return e;
    }

    *file = f;
    return 0;
}

llfs_error llfs_fclose(llfs_file *file) {
    if (file == NULL) return 0;
    llfs_error e = llfs_destroy_file(file);
    free(file);
    return e;
}

llfs_error llfs_mkdir(char *path) {
    return llfs_create_file(path, DIR);
}

llfs_error llfs_touch(char *path) {
    return llfs_create_file(path, FLAT);
}

llfs_error llfs_fread(char *buffer, int size, int count, llfs_file *file) {
    if (file == NULL) return FILE_NOT_ALLOCATED_ERROR;
    return llfs_get_bytes(file, buffer, size * count, 0);
}

llfs_error llfs_fwrite(char *content, int size, int count, llfs_file *file) {
    if (file == NULL) return FILE_NOT_ALLOCATED_ERROR;
    int left_to_write = count * size;

    llfs_error e;
    while (left_to_write > 0) {
        if (left_to_write >= MAX_WRITE_SIZE) {
            e = llfs_write(content, 1, MAX_WRITE_SIZE, file);
            if (e != 0) break;
        } else {
            e = llfs_write(content, 1, left_to_write, file);
            if (e != 0) break;
        }

        left_to_write -= MAX_WRITE_SIZE;
        content += MAX_WRITE_SIZE;
    }

    return 0;
}

llfs_error llfs_rm(char *path, int recursive) {
    return llfs_delete(path, recursive);
}