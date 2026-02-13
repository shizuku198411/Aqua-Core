#include "process.h"
#include "syscall.h"
#include "fs.h"
#include "rtc.h"


int syscall(int sysno, int arg0, int arg1, int arg2) {
    register int a0 __asm__("a0") = arg0;
    register int a1 __asm__("a1") = arg1;
    register int a2 __asm__("a2") = arg2;
    register int a3 __asm__("a3") = sysno;

    __asm__ __volatile__("ecall"
                         : "=r"(a0)
                         : "r"(a0), "r"(a1), "r"(a2), "r"(a3)
                         : "memory");

    return a0;
}


void putchar(char ch) {
    syscall(SYSCALL_PUTCHAR, ch, 0, 0);
}


long getchar(void) {
    return syscall(SYSCALL_GETCHAR, 0, 0, 0);
}


int ps(int index, struct ps_info *info) {
    return syscall(SYSCALL_PS, index, (int) info, 0);
}


int clone(int app_id) {
    return syscall(SYSCALL_CLONE, app_id, 0, 0);
}

int spawn(int app_id) {
    return clone(app_id);
}

int waitpid(int pid) {
    return syscall(SYSCALL_WAITPID, pid, 0, 0);
}

int ipc_send(int pid, int message) {
    return syscall(SYSCALL_IPC_SEND, pid, message, 0);
}

int ipc_recv(int *from_pid) {
    return syscall(SYSCALL_IPC_RECV, (int) from_pid, 0, 0);
}

int bitmap(int index) {
    return syscall(SYSCALL_BITMAP, index, 0, 0);
}

int kill(int pid) {
    return syscall(SYSCALL_KILL, pid, 0, 0);
}

int kernel_info(struct kernel_info *out) {
    return syscall(SYSCALL_KERNEL_INFO, (int) out, 0, 0);
}

__attribute__((noreturn))
void exit(void) {
    syscall(SYSCALL_EXIT, 0, 0, 0);
    __builtin_unreachable();
}


int fs_open(const char *path, int flags) {
    return syscall(SYSCALL_OPEN, (int) path, flags, 0);
}

int fs_close(int fd) {
    return syscall(SYSCALL_CLOSE, fd, 0, 0);
}

int fs_read(int fd, void *buf, int size) {
    return syscall(SYSCALL_READ, fd, (int) buf, size);
}

int fs_write(int fd, const void *buf, int size) {
    return syscall(SYSCALL_WRITE, fd, (int) buf, size);
}

int fs_mkdir(const char *path) {
    return syscall(SYSCALL_MKDIR, (int) path, 0, 0);
}

int fs_readdir(const char *path, int index, struct fs_dirent *out) {
    return syscall(SYSCALL_READDIR, (int) path, index, (int) out);
}

int fs_unlink(const char *path) {
    return syscall(SYSCALL_UNLINK, (int) path, 0, 0);
}

int fs_rmdir(const char *path) {
    return syscall(SYSCALL_RMDIR, (int) path, 0, 0);
}

int gettime(struct time_spec *out) {
    return syscall(SYSCALL_GETTIME, (int) out, 0, 0);
}

int fork(void) {
    return syscall(SYSCALL_FORK, 0, 0, 0);
}

int exec(int app_id) {
    return syscall(SYSCALL_EXEC, app_id, 0, 0);
}

int dup2(int fd1, int fd2) {
    return syscall(SYSCALL_DUP2, fd1, fd2, 0);
}