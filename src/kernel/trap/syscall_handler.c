#include "kernel.h"
#include "syscall.h"
#include "syscall_internal.h"
#include "commonlibs.h"

void handle_syscall(struct trap_frame *f) {
    switch (f->a3) {
        case SYSCALL_PUTCHAR:
            syscall_handle_putchar(f);
            break;

        case SYSCALL_GETCHAR:
            syscall_handle_getchar(f);
            break;

        case SYSCALL_EXIT:
            syscall_handle_exit(f);
            return;

        case SYSCALL_PS:
            syscall_handle_ps(f);
            break;

        case SYSCALL_CLONE:
            syscall_handle_clone(f);
            break;

        case SYSCALL_WAITPID:
            syscall_handle_waitpid(f);
            break;

        case SYSCALL_IPC_SEND:
            syscall_handle_ipc_send(f);
            break;

        case SYSCALL_IPC_RECV:
            syscall_handle_ipc_recv(f);
            break;

        case SYSCALL_BITMAP:
            syscall_handle_bitmap(f);
            break;

        case SYSCALL_KILL:
            syscall_handle_kill(f);
            break;

        case SYSCALL_KERNEL_INFO:
            syscall_handle_kernel_info(f);
            break;

        case SYSCALL_OPEN:
            syscall_handle_open(f);
            break;

        case SYSCALL_CLOSE:
            syscall_handle_close(f);
            break;

        case SYSCALL_READ:
            syscall_handle_read(f);
            break;

        case SYSCALL_WRITE:
            syscall_handle_write(f);
            break;

        case SYSCALL_MKDIR:
            syscall_handle_mkdir(f);
            break;

        case SYSCALL_READDIR:
            syscall_handle_readdir(f);
            break;

        case SYSCALL_UNLINK:
            syscall_handle_unlink(f);
            break;

        case SYSCALL_RMDIR:
            syscall_handle_rmdir(f);
            break;

        case SYSCALL_GETTIME:
            syscall_handle_gettime(f);
            break;

        default:
            PANIC("undefined system call");
    }
}
