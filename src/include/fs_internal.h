#pragma once

#include "fs.h"

void fs_init(void);
int fs_open(int pid, const char *path, int flags);
int fs_close(int pid, int fd);
int fs_read(int pid, int fd, void *buf, size_t size);
int fs_write(int pid, int fd, const void *buf, size_t size);
int fs_mkdir(const char *path);
int fs_readdir(const char *path, int index, struct fs_dirent *out);
int fs_unlink(const char *path);
int fs_rmdir(const char *path);
void fs_on_process_recycle(int pid);
uint32_t fs_get_pfs_image_blocks(void);
