//
// Created by curt white on 2020-03-17.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "error.h"
#include "../disk/disk.h"
#include "journal.h"

#define jindex(val) ((val) % (JOURNAL_LENGTH - 1)) + JOURNAL_LOG_START

typedef enum checksum_method {
    CRC32
} checksum_method;

// In memory copy of the journal super block
journal_super super;

llfs_error journal_create_checksum() {
    // TODO: Create the journal checksum values
    return 0;
}

llfs_error journal_check_checksum() {
    // TODO: Validate the journal checksum values
    return 0;
}

/**
 * Write the blocks list provided to the disk
 * @param blocks - The blocks to be written
 * @param num_blocks - The number of blocks
 * @return - llfs_error or 0 for success
 */
llfs_error write_blocks(file_block *blocks, int num_blocks) {
    disk_error e = 0;
    for (int i = 0; i < num_blocks; i ++) {
        e = disk_write_block(blocks[i].block_num, blocks[i].block_data);
        if (e != 0) { return DISK_ERROR; }
    }

    return 0;
}

/**
 * Commit all of the journals blocks to the disk
 * @return - llfs_error or 0 for success
 */
llfs_error journal_transaction_commit() {
    char *buffer = (char *) calloc(BLOCK_SIZE, sizeof(char));
    if (buffer == NULL) return MEMORY_ALLOC_ERROR;

    disk_error e = disk_read_block(jindex(super.log_start), buffer);
    if (e != 0) { free(buffer); return DISK_ERROR; }

    journal_descriptor jd;
    memcpy(&jd, buffer, sizeof(journal_descriptor));
    if (jd.block_type != JOURNAL_DESCRIPTOR) { free(buffer); return JOURNAL_ERROR; }

    e = disk_read_block(jindex(super.log_start + jd.num_blocks + 1), buffer);
    if (e != 0) { free(buffer); return DISK_ERROR; }

    journal_commit cm;
    memcpy(&cm, buffer, sizeof(journal_commit));
    if (cm.block_type != JOURNAL_COMMIT) { free(buffer); return JOURNAL_ERROR; }

    file_block *blocks = (file_block *) calloc(jd.num_blocks, sizeof(file_block));
    if (blocks == NULL) { free(buffer); return MEMORY_ALLOC_ERROR; }

    int i;
    for (i = 0; i < jd.num_blocks; i++) {
        blocks[i].block_data = calloc(BLOCK_SIZE, sizeof(char));
        if (blocks[i].block_data == NULL) break;
        e = disk_read_block(jindex(super.log_start + i + 1), blocks[i].block_data);
        if (e != 0) break;

        blocks[i].block_num = jd.blocks[i];
    }

    llfs_error err = 0;
    if (i == jd.num_blocks) err = write_blocks(blocks, jd.num_blocks);

    memset(buffer, 0, BLOCK_SIZE);
    e = disk_write_block(jindex(super.log_start + jd.num_blocks + 2), buffer);
    if (e != 0) err = DISK_ERROR;

    super.log_start = (super.log_start + jd.num_blocks + 2) % (JOURNAL_LENGTH - 1);
    memcpy(buffer, &super, sizeof(journal_super));
    e = disk_write_block(JOURNAL_LOCATION, buffer);
    if (e != 0) err = DISK_ERROR;

    for (int j = 0; j < i; ++j) free(blocks[j].block_data);
    free(blocks);
    free(buffer);

    return err;
}

/**
 * Create a new journal transaction and store all of the provided blocks in the journal
 * @param blocks - A list of file blocks to store
 * @param num_blocks - The number of blocks in the first argument
 * @return - llfs_error or 0 for success
 */
llfs_error journal_new_transaction(file_block *blocks, int num_blocks) {
    if (num_blocks > MAX_TRANSACTION_LEN) return JOURNAL_ERROR;

    journal_descriptor desc = { JOURNAL_DESCRIPTOR, 0, num_blocks, { 0 } };
    for (int i = 0; i < num_blocks; i++) desc.blocks[i] = blocks[i].block_num;

    char buffer[BLOCK_SIZE] = { 0 };
    memcpy(buffer, &desc, sizeof(journal_descriptor));

    disk_error e = disk_write_block(jindex(super.log_start), buffer);
    if (e != 0) return DISK_ERROR;

    for (int i = 0; i < num_blocks; i ++) {
        e = disk_write_block(jindex(super.log_start + i + 1), blocks[i].block_data);
        if (e != 0) return DISK_ERROR;
    }

    journal_commit cm = { JOURNAL_COMMIT, 0, 0 };
    memcpy(buffer, &cm, sizeof(journal_commit));
    e = disk_write_block(jindex(super.log_start + num_blocks + 1), buffer);
    if (e != 0) return DISK_ERROR;

    llfs_error err = journal_transaction_commit();
    return err;
}

/**
 * Initialize the journal with starting values to be called when the disk is
 * being formatted.
 * @return - llfs_error or 0 for success
 */
llfs_error journal_init() {
    char buffer[BLOCK_SIZE] = { 0 };

    journal_super s = { 0, JOURNAL_LOCATION, CRC32, JOURNAL_LENGTH, MAX_TRANSACTION_LEN, 0};
    memcpy(buffer, &s, sizeof(journal_super));
    disk_error e = disk_write_block(JOURNAL_LOCATION, buffer);
    if (e != 0) return DISK_ERROR;

    llfs_error err = 0;
    memset(buffer, 0, BLOCK_SIZE);
    e = disk_write_block(JOURNAL_LOG_START, buffer);
    if (e != 0) err = DISK_ERROR;

    super = s;
    return err;
}

/**
 * Recover the journal data from a crash. Simply replay the last
 * @return
 */
llfs_error journal_recover() {
    char buffer[BLOCK_SIZE] = { 0 };

    disk_error e = disk_read_block(JOURNAL_LOCATION, buffer);
    if (e != 0) return DISK_ERROR;

    memcpy(&super, buffer, sizeof(journal_super));

    llfs_error err = journal_transaction_commit();
    // If error entry is incomplete and will be ignored or no entry is present
    if (err == JOURNAL_ERROR) err = 0;

    return err;
}
