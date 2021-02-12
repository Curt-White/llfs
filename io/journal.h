//
// Created by curt white on 2020-03-17.
//


#ifndef JOURNAL_INCLUDED
#define JOURNAL_INCLUDED

#include <stdint.h>
#include "error.h"

#define JOURNAL_DESCRIPTOR 1
#define JOURNAL_COMMIT 2

#define MAX_TRANSACTION_LEN 10
#define JOURNAL_LOCATION 12
#define JOURNAL_LOG_START JOURNAL_LOCATION + 1
#define JOURNAL_LENGTH 20

// Some blocks need to be referenced instead of owned so this is how we
// can ensure they dont get referenced twice
typedef enum fb_type {
    FB_OWNED,
    FB_REF
} fb_type;

typedef struct file_block {
    int32_t block_num;
    char *block_data;
    fb_type t;
} file_block;

typedef struct journal_descriptor {
    uint32_t block_type;                  // Block type: Descriptor
    uint32_t seq_num;                     // Transaction sequence number
    uint32_t num_blocks;                  // Number of blocks in the transaction
    uint32_t blocks[MAX_TRANSACTION_LEN]; // Block location of each piece of data in transaction
} journal_descriptor;

typedef struct journal_commit {
    uint32_t block_type;     // Block type: Commit
    uint32_t checksum;       // Checksum for all of the data in transaction
    uint32_t time;           // time written to journal
} journal_commit;

typedef struct journal_super {
    uint32_t log_start;              // Start of the log in the journal, 0 is block after super
    uint32_t block_start;            // Block number of journal start
    uint32_t checksum_type;          // Method for journal checksums
    uint32_t block_count;            // Number of blocks in journal
    uint32_t max_transaction_len;    // Max length of transaction
    uint32_t checksum;               // Checksum value of journal super block
} journal_super;

llfs_error journal_new_transaction(file_block *blocks, int num_blocks);
llfs_error journal_init();
llfs_error journal_recover();

#endif