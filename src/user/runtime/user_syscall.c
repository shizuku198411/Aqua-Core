#include "process.h"
#include "syscall.h"


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

__attribute__((noreturn))
void exit(void) {
    syscall(SYSCALL_EXIT, 0, 0, 0);
    __builtin_unreachable();
}
