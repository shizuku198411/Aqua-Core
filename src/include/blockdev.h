#pragma once

#include "stdtypes.h"

#define BLOCKDEV_BLOCK_SIZE 512
#define BLOCKDEV_BLOCK_COUNT 256

void blockdev_init(void);
int blockdev_read(uint32_t block_index, void *out_block);
int blockdev_write(uint32_t block_index, const void *in_block);
