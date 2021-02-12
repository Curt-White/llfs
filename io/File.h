//
// Created by curt white on 2020-03-11.
//

#ifndef FILE_INCLUDED
#define FILE_INCLUDED

#include "error.h"

typedef struct llfs_file llfs_file;

/**
 * Format the LLFS file system.
 * @return - llfs_error - An error or 0 for success
 */
llfs_error InitLLFS();

// This is being redefined wit a new name
typedef enum llfs_seek_opt {
    LLFS_FSEEK_START,
    LLFS_FSEEK_END,
    LLFS_FSEEK_SET
} llfs_seek_opt;

/**
 * Seek to a position in a file
 * @param file - A file to seek a new position in
 * @param p - The type of seek (end, start, set)
 * @param offset - Offset for set option. Is ignored for start and end options
 * @return - llfs_error - An error or 0 for success
 */
llfs_error llfs_fseek(llfs_file *file, llfs_seek_opt p, int offset);

/**
 * Open a file into the file pointer provided. Memory is allocated to that pointer
 * so it must be freed using the llfs_fclose functions.
 * @param path - The path of the file to open
 * @param file - A pointer to store the file data in.
 * @return - llfs_error - An error or 0 for success
 */
llfs_error llfs_fopen(char *path, llfs_file **file);

/**
 * Closes and frees the memory associated with the file pointer provided.
 * @param file - The file to close
 * @return - llfs_error - An error or 0 for success
 */
llfs_error llfs_fclose(llfs_file *file);

/**
 * Make a new directory at the path provided. The function will not create new
 * directories to store the new file so it must be created in an existing directory.
 * The path must be absolute.
 * @param path - The path of the file to create the directory in
 * @return - llfs_error - An error or 0 for success
 */
llfs_error llfs_mkdir(char *path);

/**
 * Create a new flat file. The path provided must be an absolute path and the containing
 * directory must exist.
 * @param path - Absolute path to the new file
 * @return - llfs_error - An error or 0 for success
 */
llfs_error llfs_touch(char *path);

/**
 * Reads from the file pointer provided. If the pointer is null an error will be returned.
 * @param buffer - Buffer will store the data
 * @param size - The size of a single item being read
 * @param count - The number of items being read
 * @param file - The file to read data from
 * @return - llfs_error - An error or 0 for success
 */
llfs_error llfs_fread(char *buffer, int size, int count, llfs_file *file);

/**
 * There us a maximum buffer size of 2kb. If exceeded an error will be returned. If the file pointer
 * is null an error will be returned.
 * @param content - Content store the data being written
 * @param size - The size of a single item being written
 * @param count - The number of items being written
 * @param file - The file to write to
 * @return - llfs_error - An error or 0 for success
 */
llfs_error llfs_fwrite(char *content, int size, int count, llfs_file *file);

/**
 * Remove a directory or file at the absolute path provided. If the recursive flag is set then
 * if the file is a directory, all of its children will be removed. If the recursive flag is not
 * set empty directories and flat files may be deleted but an error will be returned when trying to
 * remove a non-empty directory.
 * @param path - Path to the file to remove
 * @param recursive - Flag. 0 = non-recursive and 1 = recursive
 * @return - llfs_error - An error or 0 for success
 */
llfs_error llfs_rm(char *path, int recursive);

#endif