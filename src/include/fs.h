#pragma once

#include "stdtypes.h"

#define FS_PATH_MAX      64
#define FS_NAME_MAX      16
#define FS_FD_MAX        16
#define FS_MAX_NODES     128
#define FS_FILE_MAX_SIZE 512

#define FS_TYPE_FILE 1
#define FS_TYPE_DIR  2

#define O_RDONLY 0x1
#define O_WRONLY 0x2
#define O_RDWR   (O_RDONLY | O_WRONLY)
#define O_CREAT  0x10
#define O_TRUNC  0x20

struct fs_dirent {
    char name[FS_NAME_MAX];
    int type;
    uint32_t size;
};

#define STDIN   0
#define STDOUT  1