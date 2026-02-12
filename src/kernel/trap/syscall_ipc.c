#include "syscall_internal.h"
#include "process.h"

#define SSTATUS_SUM (1u << 18)

extern struct process *current_proc;

static void write_user_int(int *user_ptr, int value) {
    uint32_t sstatus = READ_CSR(sstatus);
    WRITE_CSR(sstatus, sstatus | SSTATUS_SUM);
    *user_ptr = value;
    WRITE_CSR(sstatus, sstatus);
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
        write_user_int(from_pid_ptr, from_pid);
    }

    f->a0 = (int) message;
}
