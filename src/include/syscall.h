#pragma once

#include "kernel.h"

#define SYSCALL_PUTCHAR     1
#define SYSCALL_GETCHAR     2
#define SYSCALL_EXIT        3
#define SYSCALL_PS          4
#define SYSCALL_CLONE       5
#define SYSCALL_BITMAP      6
#define SYSCALL_WAITPID     7
#define SYSCALL_IPC_SEND    8
#define SYSCALL_IPC_RECV    9
#define SYSCALL_SPAWN       SYSCALL_CLONE
#define SYSCALL_KILL        10
#define SYSCALL_KERNEL_INFO 11
#define SYSCALL_OPEN        12
#define SYSCALL_CLOSE       13
#define SYSCALL_READ        14
#define SYSCALL_WRITE       15
#define SYSCALL_MKDIR       16
#define SYSCALL_READDIR     17
#define SYSCALL_UNLINK      18
#define SYSCALL_RMDIR       19
#define SYSCALL_GETTIME     20
#define SYSCALL_FORK        21
#define SYSCALL_EXEC        22
#define SYSCALL_DUP2        23
#define SYSCALL_EXECV       24
#define SYSCALL_GETARGS     25
#define SYSCALL_GETROOTFS   26
#define SYSCALL_GETCWD      27
#define SYSCALL_CHDIR       28


void handle_syscall(struct trap_frame *f);
void poll_console_input(void);
