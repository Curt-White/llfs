//
// Created by curt white on 2020-03-09.
//
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "unit_test.h"
#include "../disk/disk.h"

const char *test_mount() {
    disk_error res = disk_mount("other_disk");
    unit_assert(disk_strerror(res), res == DISK_ALREADY_LOADED);

    char write_block[512] = "writing some data";
    res = disk_write_block(18, (char *) write_block);
    unit_assert(disk_strerror(res), res == 0);

    res = disk_unmount();
    unit_assert(disk_strerror(res), res == 0);

    res = disk_mount("vdisk");
    unit_assert(disk_strerror(res), res == 0);

    char read_block[512] = { 0 };
    res = disk_read_block(18, (char *) read_block);
    unit_assert(disk_strerror(res), res == 0);
    unit_assert("Data Was Not Persisted On Disk Unmount", strcmp(write_block, read_block) == 0);

    res = disk_unmount();
    unit_assert(disk_strerror(res), res == 0);

    res = disk_unmount();
    unit_assert(disk_strerror(res), res == DISK_NOT_LOADED);

    pass();
    return 0;
}

int main() {
    disk_mount("vdisk");

    test_header();
    unit tests[] = {
        test_mount
    };

    const char *msg = run_tests(tests, sizeof(tests) / sizeof(unit));
    if (msg != NULL) {
        fail_msg(msg);
    } else {
        pass_all();
    }

    return 0;
}
