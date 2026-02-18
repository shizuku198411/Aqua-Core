#pragma once

#include "kernel/kernel.h"
#include "proc/process.h"
#include "fs/fs.h"
#include "time/rtc.h"

void putchar(char ch);
long getchar(void);
int ps(int index, struct ps_info *info);
int clone(int app_id);
int spawn(int app_id);
int waitpid(int pid, int *status, int options);
int waitpid_opts(int pid, int options);
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
int gettime(struct time_spec *out);
int sleep(uint32_t ms);
int fork(void);
int exec(int app_id);
int execv(int app_id, const char **argv);
int exec_path(const char *path);
int execv_path(const char *path, const char **argv);
int dup2(int old_fd, int new_fd);
int getargs(struct exec_args *out);
int getcwd(char *cwd_path);
int chdir(const char *path);
int ping_tx(uint32_t dst_ip, uint16_t id, uint16_t seq);
__attribute__((noreturn)) void exit(int status);
