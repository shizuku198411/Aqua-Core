#pragma once

#include "kernel.h"

void syscall_handle_putchar(struct trap_frame *f);
void syscall_handle_getchar(struct trap_frame *f);
void syscall_handle_exit(struct trap_frame *f);
void syscall_handle_ps(struct trap_frame *f);
void syscall_handle_clone(struct trap_frame *f);
void syscall_handle_waitpid(struct trap_frame *f);
void syscall_handle_ipc_send(struct trap_frame *f);
void syscall_handle_ipc_recv(struct trap_frame *f);
void syscall_handle_bitmap(struct trap_frame *f);
void syscall_handle_kill(struct trap_frame *f);
void syscall_handle_kernel_info(struct trap_frame *f);
void syscall_handle_open(struct trap_frame *f);
void syscall_handle_close(struct trap_frame *f);
void syscall_handle_read(struct trap_frame *f);
void syscall_handle_write(struct trap_frame *f);
void syscall_handle_mkdir(struct trap_frame *f);
void syscall_handle_readdir(struct trap_frame *f);
void syscall_handle_unlink(struct trap_frame *f);
void syscall_handle_rmdir(struct trap_frame *f);
