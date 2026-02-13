#pragma once

#include "kernel.h"
#include "process.h"
#include "fs.h"

void putchar(char ch);
long getchar(void);
int ps(int index, struct ps_info *info);
int clone(int app_id);
int spawn(int app_id);
int waitpid(int pid);
int ipc_send(int pid, int message);
int ipc_recv(int *from_pid);
int bitmap(int index);
int kill(int pid);
int kernel_info(struct kernel_info *out);
int fs_open(const char *path, int flags);
int fs_close(int fd);
int fs_read(int fd, void *buf, int size);
int fs_write(int fd, const void *buf, int size);
int fs_mkdir(const char *path);
int fs_readdir(const char *path, int index, struct fs_dirent *out);
int fs_unlink(const char *path);
int fs_rmdir(const char *path);
__attribute__((noreturn)) void exit(void);
