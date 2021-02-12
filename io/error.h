//
// Created by curt white on 2020-03-18.
//

#ifndef ERROR_INCLUDED
#define ERROR_INCLUDED

// Return an error if one occurs, Realise this is bad practice
// but felt like it needed something like ? in rust
#define unwrap(e) do { llfs_error ___err___ = e; if(___err___ != 0) return ___err___; } while(0)

typedef enum llfs_error {
    MEMORY_ALLOC_ERROR = 1,
    DISK_ERROR,
    DISK_FULL_ERROR,
    EMPTY_FILE_ERROR,
    BUFFER_DUPLICATE_ERROR,
    INVALID_OPTION_ERROR,
    BAD_PATH_ERROR,
    BYTE_OUT_OF_RANGE_ERROR,
    END_OF_FILE_ERROR,
    FILE_FULL_ERROR,
    FILE_NOT_FOUND_ERROR,
    NON_RECURSIVE_DELETE_ERROR,
    INODE_FREE_ERROR,
    EXCEEDED_MAX_BUFFER_SIZE,
    FILE_NOT_ALLOCATED_ERROR,
    FILE_ALREADY_EXISTS_ERROR,
    JOURNAL_ERROR,
    JOURNAL_BAD_HEADER
} llfs_error;

const char *llfs_strerror(llfs_error e);

#endif