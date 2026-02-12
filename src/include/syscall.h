#pragma once

#include "kernel.h"

#define SYSCALL_PUTCHAR     1
#define SYSCALL_GETCHAR     2
#define SYSCALL_EXIT        3
#define SYSCALL_PS          4
#define SYSCALL_CLONE       5
#define SYSCALL_BITMAP      6
#define SYSCALL_SPAWN       SYSCALL_CLONE

#define APP_ID_SHELL        1
#define APP_ID_PS           2

void handle_syscall(struct trap_frame *f);
void poll_console_input(void);
