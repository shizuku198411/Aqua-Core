#pragma once

#include "fs.h"

void fs_init(void);
int fs_fork_copy_fds(int parent_pid, int child_pid);
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
int fs_dup2(int pid, int old_fd, int new_fd);
int fs_get_root_entry(int *mount_idx, int *node_idx);
int fs_get_path_entry(int *mount_idx, int *node_idx, const char *path);