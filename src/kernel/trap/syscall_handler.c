#include "kernel/kernel.h"
#include "user/syscall.h"
#include "syscall_internal.h"
#include "core/commonlibs.h"

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
        
        case SYSCALL_FORK:
            syscall_handle_fork(f);
            break;

        case SYSCALL_DUP2:
            syscall_handle_dup2(f);
            break;

        case SYSCALL_GETARGS:
            syscall_handle_getargs(f);
            break;

        case SYSCALL_GETCWD:
            syscall_handle_getcwd(f);
            break;

        case SYSCALL_CHDIR:
            syscall_handle_chdir(f);
            break;

        case SYSCALL_PING_TX:
            syscall_handle_ping_tx(f);
            break;

        case SYSCALL_SLEEP:
            syscall_handle_sleep(f);
            break;

        case SYSCALL_EXEC_PATH:
            syscall_handle_exec_path(f);
            break;

        case SYSCALL_EXECV_PATH:
            syscall_handle_execv_path(f);
            break;

        case SYSCALL_SOCKET:
            syscall_handle_socket(f);
            break;

        case SYSCALL_SENDTO:
            syscall_handle_sendto(f);
            break;

        case SYSCALL_RECVFROM:
            syscall_handle_recvfrom(f);
            break;

        case SYSCALL_BIND:
            syscall_handle_bind(f);
            break;

        default:
            PANIC("undefined system call");
    }
}
