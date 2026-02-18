#include "kernel/kernel.h"
#include "syscall_internal.h"
#include "proc/process.h"
#include "kernel/page_access.h"

extern struct process *current_proc;

static int write_user_int(int *user_ptr, int value) {
    if (copyout(user_ptr, &value, sizeof(value)) < 0) {
        return -1;
    }
    return 0;
}

void syscall_handle_ipc_send(struct trap_frame *f) {
    int dst_pid = (int) f->a0;
    uint32_t message = f->a1;

    if (!current_proc || current_proc->pid <= 0) {
        f->a0 = -1;
        return;
    }

    f->a0 = process_ipc_send(current_proc->pid, dst_pid, message);
}

void syscall_handle_ipc_recv(struct trap_frame *f) {
    int *from_pid_ptr = (int *) f->a0;
    int from_pid = 0;
    uint32_t message = 0;

    if (!current_proc || current_proc->pid <= 0) {
        f->a0 = -1;
        return;
    }

    int ret = process_ipc_recv(current_proc->pid, &from_pid, &message);
    if (ret < 0) {
        f->a0 = ret;
        return;
    }

    if (from_pid_ptr) {
        if (write_user_int(from_pid_ptr, from_pid) < 0) {
            f->a0 = -1;
            return;
        }
    }

    f->a0 = (int) message;
}
