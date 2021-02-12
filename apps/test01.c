//
// Created by curt white on 2020-03-09.
//
#include <stdio.h>
#include <string.h>

#include "../disk/disk.h"
#include "unit_test.h"

const char *test_read_write() {
    char write_block[512] = "Hello World!";
    disk_error res = disk_write_block(10, (char *) write_block);
    unit_assert(disk_strerror(res), res == 0);

    char read_block[512];
    res = disk_read_block(10, (char *) read_block);

    unit_assert(disk_strerror(res), res == 0);
    unit_assert("Incorrect Data Found", strncmp((const char *)read_block, "Hello", 5) == 0);

    res = disk_write_block(-1, (char *) write_block);
    unit_assert(disk_strerror(res), res == BLOCK_OUT_OF_BOUNDS);

    res = disk_write_block(4096, (char *) write_block);
    unit_assert(disk_strerror(res), res == BLOCK_OUT_OF_BOUNDS);

    pass();
    return 0;
}

int main() {
    disk_mount("vdisk");

    test_header();
    unit tests[] = {
        test_read_write,
    };

    const char *msg = run_tests(tests, sizeof(tests) / sizeof(unit));
    if (msg != NULL) {
        fail_msg(msg);
    } else {
        pass_all();
    }
}
