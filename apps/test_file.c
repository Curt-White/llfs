//
// Created by curt white on 2020-03-29.
//
#include <stdio.h>
#include <stdlib.h>

#include "../io/File.h"
#include "../disk/disk.h"
#include "unit_test.h"

const char *test_mkdir() {
    llfs_error e = llfs_mkdir("/");
    unit_assert(llfs_strerror(e), e == BAD_PATH_ERROR);

    e = llfs_mkdir("/usr");
    unit_assert(llfs_strerror(e), e == 0);

    e = llfs_mkdir("/usr");
    unit_assert(llfs_strerror(e), e == FILE_ALREADY_EXISTS_ERROR);

    e = llfs_mkdir("/usr/curtwhite");
    unit_assert(llfs_strerror(e), e == 0);

    e = llfs_mkdir("/empty_dir");
    unit_assert(llfs_strerror(e), e == 0);

    e = llfs_mkdir("/lib/python");
    unit_assert(llfs_strerror(e), e == BAD_PATH_ERROR);

    pass(); return 0;
}

const char *test_touch() {
    llfs_error e = llfs_touch("/test.c");
    unit_assert(llfs_strerror(e), e == 0);

    e = llfs_touch("/usr/curtwhite/file.c");
    unit_assert(llfs_strerror(e), e == 0);

    e = llfs_touch("/");
    unit_assert(llfs_strerror(e), e == BAD_PATH_ERROR);

    e = llfs_touch("/lib/file.c");
    unit_assert(llfs_strerror(e), e == BAD_PATH_ERROR);

    e = llfs_touch("/lib/my_file_name_is_over_thirty_one_characters_long.c");
    unit_assert(llfs_strerror(e), e == BAD_PATH_ERROR);

    pass(); return 0;
}

const char *test_fopen() {
    llfs_file *file;
    llfs_error e = llfs_fopen("/test.c", &file);
    unit_assert(llfs_strerror(e), e == 0);

    e = llfs_fclose(file);
    unit_assert(llfs_strerror(e), e == 0);

    e = llfs_fopen("/non_existent_file.c", &file);
    unit_assert(llfs_strerror(e), e == FILE_NOT_FOUND_ERROR);
    unit_assert("File Not NULL", file == NULL);

    pass(); return 0;
}

// Test writing to the file with various amounts of data
const char *test_fwrite() {
    FILE *test_file = fopen("./data_files/classes.xml", "r");
    if (test_file == NULL) return "Error Opening Test File";
    int test_size[] = {
            0,                      // Test writing nothing
            BLOCK_SIZE * 10,        // Testing writing to indirect block transition point
            BLOCK_SIZE * 20,        // Test writing into single indirect
            BLOCK_SIZE * 140 + 10,  // test writing into double indirect
    };

    char *paths[] = {
            "/usr/curtwhite/file.c",    // Write nothing to nested
            "/test.c",                  // Write file in root
            "/test.c",                  // Write to file in root
            "/usr/curtwhite/file.c"     // Write to nested file
    };

    for (int j = 0; j < sizeof(test_size) / sizeof(int); j++) {
        char *buffer = (char *) calloc(test_size[j], sizeof(char));
        if (buffer == NULL) return llfs_strerror(MEMORY_ALLOC_ERROR);

        size_t read = fread(buffer, sizeof(char), test_size[j], test_file);
        if (read != test_size[j]) {
            free(buffer);
            return "Error Reading Bytes From File";
        }

        llfs_file *file;
        llfs_error e = llfs_fopen(paths[j], &file);
        if (e != 0) free(buffer);
        unit_assert(llfs_strerror(e), e == 0);

        e = llfs_fwrite(buffer, sizeof(char), test_size[j], file);
        if (e != 0) free(buffer);
        unit_assert(llfs_strerror(e), e == 0);

        // Close and open the file again so we read from file instead of in memory file
        e = llfs_fclose(file);
        if (e != 0) free(buffer);
        unit_assert(llfs_strerror(e), e == 0);

        e = llfs_fopen(paths[j], &file);
        if (e != 0) free(buffer);
        unit_assert(llfs_strerror(e), e == 0);

        char *file_data = (char *) calloc(test_size[j], sizeof(char));
        if (file_data == NULL) {
            free(buffer);
            return llfs_strerror(MEMORY_ALLOC_ERROR);
        }

        e = llfs_fread(file_data, sizeof(char), test_size[j], file);
        if (e != 0 && e != END_OF_FILE_ERROR) { free(buffer); free(file_data); }
        unit_assert(llfs_strerror(e), e == 0 || e == END_OF_FILE_ERROR);

        int pass = 0;
        // Ensure the things we wrote are the same as the things we read
        for (int i = 0; i < test_size[j]; i++) {
            if (file_data[i] != buffer[i]) pass = 1;
        }

        e = llfs_fclose(file);
        free(buffer);
        free(file_data);
        unit_assert(llfs_strerror(e), e == 0);
        unit_assert("Read String Did Not Match Written One", pass == 0);
    }

    pass();
    return 0;
}

const char *test_rmdir() {
    llfs_file *file;
    // Not allowed to delete the root dir
    llfs_error e = llfs_rm("/", 0);
    unit_assert(llfs_strerror(e), e == BAD_PATH_ERROR);

    // Test rm non-existent directory
    e = llfs_rm("/lib/file.c", 0);
    unit_assert(llfs_strerror(e), e == FILE_NOT_FOUND_ERROR);
    // Try to delete a non empty directory without recursive delete
    e = llfs_rm("/usr", 0);
    unit_assert(llfs_strerror(e), e == NON_RECURSIVE_DELETE_ERROR);

    // Try to delete empty directory without recursive delete
    e = llfs_fopen("/empty_dir", &file);
    unit_assert(llfs_strerror(e), e == 0);
    e = llfs_rm("/empty_dir", 0);
    unit_assert(llfs_strerror(e), e == 0);
    e = llfs_fopen("/empty_dir", &file);
    unit_assert(llfs_strerror(e), e == FILE_NOT_FOUND_ERROR);

    // Try to directory with many files using recursive delete
    e = llfs_fopen("/usr", &file);
    unit_assert(llfs_strerror(e), e == 0);
    e = llfs_rm("/usr", 1);
    unit_assert(llfs_strerror(e), e == 0);
    e = llfs_fopen("/usr", &file);
    unit_assert(llfs_strerror(e), e == FILE_NOT_FOUND_ERROR);
    // Try opening child
    e = llfs_fopen("/usr/curtwhite", &file);
    unit_assert(llfs_strerror(e), e == FILE_NOT_FOUND_ERROR);

    e = llfs_rm("/test.c", 0);
    unit_assert(llfs_strerror(e), e == 0);

    e = llfs_fopen("/test.c", &file);
    unit_assert(llfs_strerror(e), e == FILE_NOT_FOUND_ERROR);

    pass();
    return 0;
}

int main() {
    disk_error err = disk_mount("file_disk");
    if (err != 0) {
        printf("%s\n", disk_strerror(err));
        return 0;
    }

    llfs_error e = InitLLFS();
    // Can also use load like below but the system must first be
//    llfs_error e = llfs_load();
    if (e != 0) printf("%s\n", llfs_strerror(e));

    test_header();
    unit tests[] = {
        test_mkdir,
        test_touch,
        test_fopen,
        test_fwrite,
        test_rmdir,
//         Lets run these again to show everything still preforms after removing
//         all of the previous files.
        test_mkdir,
        test_touch,
        test_fopen,
        test_fwrite,
    };

    const char *msg = run_tests(tests, sizeof(tests) / sizeof(unit));
    if (msg != NULL) {
        fail_msg(msg);
    } else {
        pass_all();
    }

    return 0;
}