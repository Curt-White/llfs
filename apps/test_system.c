#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../io/system.h"
#include "../io/File.h"
#include "../disk/disk.h"
#include "unit_test.h"

const char *test_reserve_blocks() {
    const int size = 4;
    unsigned char block_map[BLOCK_SIZE] = { 0 };
    block_map[2] |= 0b01101100u;
    int block_nums[2] = { 0 };

    llfs_error e = llfs_reserve_blocks(block_nums, 2, block_map, size);
    unit_assert(llfs_strerror(e), e == 0);
    unit_assert("Error Reserving Blocks", block_map[2] == 0b01100000u);
    unit_assert("Bad Block Numbers", block_nums[0] == 18 && block_nums[1] == 19);

    block_map[2] &= 0u;
    block_map[3] |= 0b00000011u;
    e = llfs_reserve_blocks(block_nums, 3, block_map, size);
    unit_assert(llfs_strerror(e), e == DISK_FULL_ERROR);

    e = llfs_reserve_blocks(block_nums, 2, block_map, size);
    unit_assert(llfs_strerror(e), e == 0);
    unit_assert("Error Reserving Blocks", block_map[3] == 0u);

    block_map[2] = 0b01101000u;
    free_blocks(18, block_map, size);
    unit_assert("Error Reserving Blocks", block_map[2] == 0b01101100u);
    free_blocks(20, block_map, size);
    unit_assert("Error Reserving Blocks", block_map[2] == 0b01111100u);

    // TODO: Add cross byte checking Ex start and end of block res 2

    pass();
    return 0;
}

const char *test_write_buffer() {
    llfs_write_buffer w;

    for (int i = 0; i < 20; i++) {
        file_block f = { i, NULL };
        llfs_error e  = write_buffer_append(&w, f);
        unit_assert(llfs_strerror(e), e == 0);
        unit_assert("Size Incorrect", w.num_blocks == i + 1);
        unit_assert("Bad Values", w.blocks[i].block_num == i);
    }

    file_block f = { 1, NULL };
    llfs_error e  = write_buffer_append(&w, f);
    unit_assert(llfs_strerror(e), e == BUFFER_DUPLICATE_ERROR);

    write_buffer_destroy(&w);
    pass();
    return 0;
}

const char *test_extend_file() {
    const int size = 280;
    int blocks[280] = { 0 };

    llfs_write_buffer w = { NULL, 0 };
    llfs_inode node = { 0, { FLAT }, { 0 }, 0, 0 };
    llfs_file f = { 0, 0, 0, 0, node };
    llfs_error e = llfs_extend_file(&f, &w, size, blocks);
    unit_assert(llfs_strerror(e), e == 0);
    f.inode.file_size = 143360;

    for (int i = 0; i < size; i++) {
        unit_assert("Block Number Out Of Bounds", blocks[i] < BLOCK_COUNT);
    }

    unit_assert("Bad Double Indirect Number", f.inode.double_indirect != 0);

    llfs_destroy_file(&f);
    write_buffer_destroy(&w);
    pass();
    return 0;
}

const char *test_write_file() {
    FILE *f = fopen("./data_files/test.xml", "r");
    unit_assert("Unable To Open Test File", f != NULL);

    const int amount_read = BLOCK_SIZE * 10 + 2;
    char data[BLOCK_SIZE * 10 + 2] = { 0 };
    unit_assert("Mem Alloc Error", data != NULL);

    int s = fread(data, sizeof(char), amount_read, f);
    unit_assert("Not All", s == amount_read);

    llfs_error e = llfs_create_file("/contents.txt", FLAT);
    unit_assert(llfs_strerror(e), e == 0);

    llfs_file file;
    llfs_inode i;
    int loc;
    e = llfs_get_inode("/contents.txt", &i, &loc);
    unit_assert(llfs_strerror(e), e == 0);

    e = llfs_open_file(&i, &file, loc);
    unit_assert(llfs_strerror(e), e == 0 || e == EMPTY_FILE_ERROR);

    e = llfs_fwrite(data, sizeof(char), amount_read, &file);
    unit_assert(llfs_strerror(e), e == 0);

    llfs_destroy_file(&file);

    e = llfs_get_inode("/contents.txt", &i, &loc);
    unit_assert(llfs_strerror(e), e == 0);

    e = llfs_open_file(&i, &file, loc);
    unit_assert(llfs_strerror(e), e == 0 || e == EMPTY_FILE_ERROR);

    s = fread(data, sizeof(char), 510, f);
    unit_assert("Not All", s == 510);

    e = llfs_seek(&file, LLFS_SEEK_END, 0);
    unit_assert(llfs_strerror(e), e == 0);

    e = llfs_fwrite(data, sizeof(char), 510, &file);
    unit_assert(llfs_strerror(e), e == 0);

    llfs_destroy_file(&file);
    pass();
    return 0;
}

const char *test_open_old_file() {
    llfs_file file;
    llfs_inode i;
    int loc;
    llfs_error e = llfs_get_inode("/contents.txt", &i, &loc);
    unit_assert(llfs_strerror(e), e == 0);

    e = llfs_open_file(&i, &file, loc);
    unit_assert(llfs_strerror(e), e == 0 || e == EMPTY_FILE_ERROR);

    const int amount_read = BLOCK_SIZE * 11;
    char data[BLOCK_SIZE * 11] = { 0 };
    e = llfs_get_bytes(&file, data, amount_read, 0);

    FILE *f = fopen("./data_files/test.xml", "r");
    unit_assert("Unable To Open Test File", f != NULL);

    char test_data[BLOCK_SIZE * 11] = { 0 };
    unit_assert("Mem Alloc Error", data != NULL);

    int s = fread(test_data, sizeof(char), amount_read, f);
    unit_assert("Not All", s == amount_read);

    for (int j = 0; j < amount_read; j++) {
        unit_assert("Wrong Bits", test_data[j] == data[j]);
    }

    llfs_destroy_file(&file);
    pass();
    return 0;
}

const char *test_create_file() {
    llfs_error e = llfs_create_file("/path.txt", FLAT);
    unit_assert(llfs_strerror(e), e == 0);

    e = llfs_create_file("/another_file.txt", FLAT);
    unit_assert(llfs_strerror(e), e == 0);

    e = llfs_create_file("/another_folder", DIR);
    unit_assert(llfs_strerror(e), e == 0);

    e = llfs_create_file("/another_folder/nested_file.ps", FLAT);
    unit_assert(llfs_strerror(e), e == 0);

    e = llfs_create_file("/another_folder/runner", DIR);
    unit_assert(llfs_strerror(e), e == 0);

    e = llfs_create_file("/another_folder/runner/other_file.c", DIR);
    unit_assert(llfs_strerror(e), e == 0);

    e = llfs_create_file("/another_folder/more_nesting/nested_file.ps", FLAT);
    unit_assert(llfs_strerror(e), e == BAD_PATH_ERROR);

    pass();
    return 0;
}

const char *test_open_file() {
    llfs_inode node = { 0, { 0 }, { 0 }, 0, 0 };
    int inode_block;
    llfs_error e = llfs_get_inode("/ot.txt", &node, &inode_block);
    unit_assert(llfs_strerror(e), e == FILE_NOT_FOUND_ERROR);
    pass();
    return 0;
}

const char *test_get_file_pos() {
    block_pos pos;
    llfs_error e = llfs_get_pos(-1, &pos);
    unit_assert(llfs_strerror(e), e == BYTE_OUT_OF_RANGE_ERROR);
    // First Byte
    llfs_get_pos(0, &pos);
    unit_assert("Bad Pos", pos.t == DIRECT && pos.l1 == 0 && pos.l2 == 0);
    // One Before Break
    llfs_get_pos(BLOCK_SIZE * 10 - 1, &pos);
    unit_assert("Bad Pos", pos.t == DIRECT && pos.l1 == 9 && pos.l2 == BLOCK_SIZE - 1);
    // First Indirect Byte
    llfs_get_pos(BLOCK_SIZE * 10, &pos);
    unit_assert("Bad Pos", pos.t == IND && pos.l1 == 0 && pos.l2 == 0);
    // Some Indirect Byte
    llfs_get_pos(20481, &pos);
    unit_assert("Bad Pos", pos.t == IND && pos.l1 == 30 && pos.l2 == 1);
    // Last Indirect Byte
    llfs_get_pos(70655, &pos);
    unit_assert("Bad Pos", pos.t == IND && pos.l1 == 127 && pos.l2 == 511);
    // First Double Indirect Byte
    llfs_get_pos(70656, &pos);
    unit_assert("Bad Pos", pos.t == DIND && pos.l1 == 0 && pos.l2 == 0 && pos.l3 == 0);
    // Some Double Indirect Byte
    llfs_get_pos(333825, &pos);
    unit_assert("Should Error", pos.t == DIND && pos.l1 == 4 && pos.l2 == 2 && pos.l3 == 1);
    // Try out of bound
    e = llfs_get_pos(8459264, &pos);
    unit_assert(llfs_strerror(e), e == BYTE_OUT_OF_RANGE_ERROR);

    pass();
    return 0;
}

const char *test_reserve_inode() {
    uint32_t block_map[BLOCK_SIZE / 4] = { 0 };
    int imap_block = 45;
    int inode = 0;
    llfs_error e = llfs_reserve_inode(4, block_map, BLOCK_SIZE / sizeof(uint32_t), &imap_block, &inode);
    unit_assert("Wrong Inode Block", imap_block == 0);
    unit_assert("Block Wrong In Imap", block_map[0] == 4);
    unit_assert("Wrong Inode Num", inode == 1);

    imap_block = 45;
    e = llfs_reserve_inode(345, block_map, BLOCK_SIZE / sizeof(uint32_t), &imap_block, &inode);
    unit_assert(llfs_strerror(e), e == 0);
    unit_assert("Wrong Inode Block", imap_block == 0);
    unit_assert("Block Wrong In Imap", block_map[1] == 345);
    unit_assert("Wrong Inode Num", inode == 2);

    pass();
    return 0;
}

const char *test_dir_append() {
    llfs_write_buffer w = { NULL, 0 };
    llfs_inode node = { 0, { FLAT }, { 0 }, 0, 0 };
    llfs_file f = { 0, 0, 0, 0, node };
    dir_entry dir = { 8, "something.txt" };
    llfs_error e = llfs_dir_append(&w, &f, dir);
    unit_assert(strerror(e), e == 0);

    e = llfs_seek(&f, LLFS_SEEK_START, 0);
    unit_assert(strerror(e), e == 0);

    char buffer[BLOCK_SIZE] = { 0 };
    e = llfs_get_bytes(&f, buffer, BLOCK_SIZE, 0);
    unit_assert("Bad Write", strcmp(((dir_entry *) buffer)[0].name, "something.txt") == 0);
    unit_assert(llfs_strerror(e), e == 0 || e == END_OF_FILE_ERROR);

    llfs_destroy_file(&f);
    write_buffer_destroy(&w);
    pass();
    return 0;
}

const char *test_delete_file() {
    llfs_error e = llfs_delete("/contents.txt", 0);
    unit_assert(llfs_strerror(e), e == 0);

    e = llfs_delete("/another_folder", 1);
    unit_assert(llfs_strerror(e), e == 0);

    llfs_inode inode;
    int pos;
    e = llfs_get_inode("/another_folder", &inode, &pos);
    unit_assert(llfs_strerror(e), e == FILE_NOT_FOUND_ERROR);

    pass();
    return 0;
}

int main() {
    disk_mount("system_disk");

    llfs_error e = llfs_init();
    if (e != 0) printf("%s\n", llfs_strerror(e));

    test_header();
    unit tests[] = {
        test_reserve_blocks,
        test_write_buffer,
        test_extend_file,
        test_create_file,
        test_get_file_pos,
        test_reserve_inode,
        test_dir_append,
        test_open_file,
        test_write_file,
        test_open_old_file,
        test_delete_file
    };

    const char *msg = run_tests(tests, sizeof(tests) / sizeof(unit));
    if (msg != NULL) {
        fail_msg(msg);
    } else {
        pass_all();
    }


    return 0;
}