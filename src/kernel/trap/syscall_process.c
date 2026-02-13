#include "syscall_internal.h"
#include "syscall.h"
#include "process.h"
#include "kernel.h"

#define SSTATUS_SUM (1u << 18)

extern struct process *current_proc;
extern struct process *init_proc;
extern char _binary___bin_shell_bin_start[], _binary___bin_shell_bin_size[];
extern char _binary___bin_ipc_rx_bin_start[], _binary___bin_ipc_rx_bin_size[];

static void write_user_ps_info(struct ps_info *user_ptr, const struct process *proc) {
    if (!user_ptr || !proc) {
        return;
    }

    uint32_t sstatus = READ_CSR(sstatus);
    WRITE_CSR(sstatus, sstatus | SSTATUS_SUM);

    user_ptr->pid = proc->pid;
    user_ptr->parent_pid = proc->parent_pid;
    user_ptr->state = proc->state;
    user_ptr->wait_reason = proc->wait_reason;
    for (int i = 0; i < PROC_NAME_MAX; i++) {
        user_ptr->name[i] = proc->name[i];
    }

    WRITE_CSR(sstatus, sstatus);
}

void syscall_handle_exit(struct trap_frame *f) {
    (void) f;

    // Treat pid=1 as init process. When init exits, shut down kernel.
    if (current_proc && current_proc == init_proc) {
        current_proc->state = PROC_EXITED;
        kernel_shutdown();
    }

    orphan_children(current_proc->pid);
    current_proc->state = PROC_EXITED;
    notify_child_exit(current_proc);
    yield();
}

void syscall_handle_ps(struct trap_frame *f) {
    int index = (int) f->a0;
    struct ps_info *info_ptr = (struct ps_info *) f->a1;
    if (index < 0 || index >= PROCS_MAX) {
        f->a0 = -1;
        return;
    }

    struct process *proc = &procs[index];
    write_user_ps_info(info_ptr, proc);
    f->a0 = 0;
}

void syscall_handle_clone(struct trap_frame *f) {
    const void *image = NULL;
    size_t image_size = 0;
    const char *name = NULL;

    switch ((int) f->a0) {
        case APP_ID_SHELL:
            image = _binary___bin_shell_bin_start;
            image_size = (size_t) _binary___bin_shell_bin_size;
            name = "shell";
            break;
        case APP_ID_IPC_RX:
            image = _binary___bin_ipc_rx_bin_start;
            image_size = (size_t) _binary___bin_ipc_rx_bin_size;
            name = "ipc_rx";
            break;
        default:
            f->a0 = -1;
            return;
    }

    struct process *proc = create_process(image, image_size, name);
    if (proc == NULL) {
        f->a0 = -1;
        return;
    }
    proc->parent_pid = current_proc ? current_proc->pid : 0;
    f->a0 = proc->pid;
}

void syscall_handle_waitpid(struct trap_frame *f) {
    int target_pid = (int) f->a0;
    if (target_pid <= 0 && target_pid != -1) {
        f->a0 = -1;
        return;
    }

    f->a0 = wait_for_child_exit(current_proc->pid, target_pid);
}

void syscall_handle_kill(struct trap_frame *f) {
    f->a0 = process_kill((int) f->a0);
}
