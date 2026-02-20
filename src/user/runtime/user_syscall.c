#include "proc/process.h"
#include "user/syscall.h"
#include "fs/fs.h"
#include "time/rtc.h"
#include "user/socket.h"


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

static int syscall6(int sysno, int arg0, int arg1, int arg2, int arg4, int arg5, int arg6) {
    register int a0 __asm__("a0") = arg0;
    register int a1 __asm__("a1") = arg1;
    register int a2 __asm__("a2") = arg2;
    register int a3 __asm__("a3") = sysno;
    register int a4 __asm__("a4") = arg4;
    register int a5 __asm__("a5") = arg5;
    register int a6 __asm__("a6") = arg6;

    __asm__ __volatile__("ecall"
                         : "=r"(a0)
                         : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6)
                         : "memory");
    return a0;
}


void putchar(char ch) {
    (void) syscall(SYSCALL_WRITE, 1, (int) &ch, 1);
}


long getchar(void) {
    char ch = 0;
    int n = syscall(SYSCALL_READ, 0, (int) &ch, 1);
    if (n <= 0) {
        return -1;
    }
    return (uint8_t) ch;
}


int ps(int index, struct ps_info *info) {
    return syscall(SYSCALL_PS, index, (int) info, 0);
}

int waitpid(int pid, int *status, int options) {
    return syscall(SYSCALL_WAITPID, pid, (int) status, options);
}

int waitpid_opts(int pid, int options) {
    return waitpid(pid, NULL, options);
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
void exit(int status) {
    syscall(SYSCALL_EXIT, status, 0, 0);
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

int sleep(uint32_t ms) {
    return syscall(SYSCALL_SLEEP, (int) ms, 0, 0);
}

int fork(void) {
    return syscall(SYSCALL_FORK, 0, 0, 0);
}

int exec_path(const char *path) {
    return syscall(SYSCALL_EXEC_PATH, (int) path, 0, 0);
}

int execv_path(const char *path, const char **argv) {
    return syscall(SYSCALL_EXECV_PATH, (int) path, (int) argv, 0);
}

int dup2(int old_fd, int new_fd) {
    return syscall(SYSCALL_DUP2, old_fd, new_fd, 0);
}

int getargs(struct exec_args *out) {
    return syscall(SYSCALL_GETARGS, (int) out, 0, 0);
}

int getcwd(char *cwd_path) {
    return syscall(SYSCALL_GETCWD, (int) cwd_path, 0, 0);
}

int chdir(const char *path) {
    return syscall(SYSCALL_CHDIR, (int) path, 0, 0);
}

int ping_tx(uint32_t dst_ip, uint16_t id, uint16_t seq) {
    return syscall(SYSCALL_PING_TX, (int) dst_ip, (int) id, (int) seq);
}

int socket(int domain, int type, int protocol) {
    return syscall(SYSCALL_SOCKET, domain, type, protocol);
}

int bind(int sockfd, const struct socket_addr_in *addr, uint32_t addrlen) {
    return syscall(SYSCALL_BIND, sockfd, (int) addr, (int) addrlen);
}

int sendto(int sockfd, const void *buf, int len, const struct socket_addr_in *to, uint32_t tolen) {
    return syscall6(SYSCALL_SENDTO, sockfd, (int) buf, len, (int) to, (int) tolen, 0);
}

int recvfrom(int sockfd, void *buf, int len, struct socket_addr_in *from, uint32_t *fromlen) {
    return syscall6(SYSCALL_RECVFROM, sockfd, (int) buf, len, (int) from, (int) fromlen, 0);
}
