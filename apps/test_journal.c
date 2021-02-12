//
// Created by curt white on 2020-03-31.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unit_test.h"

#include "../io/File.h"
#include "../io/journal.h"
#include "../disk/disk.h"

/**
 * Since it is so difficult to simulate a disk crashing in the
 * middle of the writing process since one function handles all the
 * writing we will manually place data and ensure recover handles it properly
 */
const char *test_recover() {
    const char * test_str = "A string to check if success";
    char write_block[BLOCK_SIZE] = { 0 };
    journal_descriptor desc = { JOURNAL_DESCRIPTOR, 0, 1, { 33 } };
    memcpy(write_block, &desc, sizeof(journal_descriptor));
    // Write the descriptor
    disk_error res = disk_write_block(JOURNAL_LOG_START, (char *) write_block);
    unit_assert(disk_strerror(res), res == 0);
    memcpy(write_block, test_str, 29);
    // Write some data
    res = disk_write_block(JOURNAL_LOG_START + 1, (char *) write_block);
    unit_assert(disk_strerror(res), res == 0);

    journal_commit cm = { JOURNAL_COMMIT, 0, 0 };
    memcpy(write_block, &cm, sizeof(journal_commit));
    // Commit the transaction
    res = disk_write_block(JOURNAL_LOG_START + 2, (char *) write_block);
    unit_assert(disk_strerror(res), res == 0);
    // Recover as if failed between committing transaction and writing to final spots
    llfs_error err = journal_recover();
    unit_assert(llfs_strerror(err), err == 0);
    // Check that the transaction was replayed onto the disk
    res = disk_read_block(33, (char *) write_block);
    unit_assert(disk_strerror(res), res == 0);
    unit_assert("Did not match", strcmp(write_block, test_str) == 0);
    // Make sure the log was advanced to the right location
    journal_super s;
    res = disk_read_block(JOURNAL_LOCATION, (char *) write_block);
    unit_assert(disk_strerror(res), res == 0);
    memcpy(&s, write_block, sizeof(journal_super));
    unit_assert("Journal Log Start In Wrong Position", s.log_start == 3);

    pass();
    return 0;
}

const char *test_blank() {
    char read_block[BLOCK_SIZE] = { 0 };
    disk_error res = disk_read_block(JOURNAL_LOG_START + 3, (char *) read_block);
    unit_assert(disk_strerror(res), res == 0);
    // Check the log position was cleared
    for (int i = 0; i < BLOCK_SIZE; i++) {
        unit_assert("Journal Log Start Not Reset", read_block[i] == 0);
    }

    pass();
    return 0;
}

/* test proper behaviour when the last written item was either corrupt or
 * not present
*/
const char *test_empty_reset() {
    llfs_error err = journal_recover();
    unit_assert(llfs_strerror(err), err == 0);
    // Ensure that the journal will not replay a corrupted log
    journal_super s;
    char read_block[BLOCK_SIZE] = { 0 };
    disk_error res = disk_read_block(JOURNAL_LOCATION, (char *) read_block);
    unit_assert(disk_strerror(res), res == 0);
    memcpy(&s, read_block, sizeof(journal_super));
    unit_assert("Journal Log Start In Wrong Position", s.log_start == 3);

    pass();
    return 0;
}

int main() {
    disk_mount("journal_disk");

    llfs_error e = InitLLFS();
    if (e != 0) printf("%s\n", llfs_strerror(e));

    test_header();
    unit tests[] = {
        test_recover,
        test_blank,
        test_empty_reset
    };

    const char *msg = run_tests(tests, sizeof(tests) / sizeof(unit));
    if (msg != NULL) {
        fail_msg(msg);
    } else {
        pass_all();
    }


    return 0;
}