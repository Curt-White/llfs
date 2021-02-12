//
// Created by curt white on 2020-03-18.
//

#include "error.h"

static const char *LLFS_ERROR_STRING[] = {
        "Error Allocating Memory",
        "Error Reading Or Writing To The Disk",
        "Disk Is Full",
        "Opened File Has No Content",
        "This Block Is Already In The Buffer",
        "An Invalid Option Was Provided",
        "The Path Name Provided Is Invalid",
        "The Byte Is Beyond File Extents",
        "The End of The File Has Been Reached",
        "File Is Full",
        "The File Was Not Found",
        "Must Use Recursive Delete If Trying To Remove Non Empty Directories",
        "Attempted To Free The Root Or Out Of Range Inode",
        "The Maximum Buffer Size Has Been Exceeded",
        "The File Provided Has Not Been Allocated",
        "A File Already Exists With The Name Provided",
        "A Journal Error Has Occurred",
        "The Journal Header Found Is Invalid"
};

const char *llfs_strerror(llfs_error e) {
    if (e == 0) return "Success";
    return LLFS_ERROR_STRING[e - 1];
}
