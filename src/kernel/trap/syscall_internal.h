#pragma once

#include "kernel.h"

void syscall_handle_putchar(struct trap_frame *f);
void syscall_handle_getchar(struct trap_frame *f);
void syscall_handle_exit(struct trap_frame *f);
void syscall_handle_ps(struct trap_frame *f);
void syscall_handle_clone(struct trap_frame *f);
void syscall_handle_bitmap(struct trap_frame *f);
