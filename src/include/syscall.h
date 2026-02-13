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

#define APP_ID_SHELL        1
#define APP_ID_IPC_RX       2

void handle_syscall(struct trap_frame *f);
void poll_console_input(void);
