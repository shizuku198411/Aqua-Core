#include "syscall_internal.h"
#include "syscall.h"
#include "process.h"

extern struct process *current_proc;
extern struct process *init_proc;
extern char _binary___bin_shell_bin_start[], _binary___bin_shell_bin_size[];
extern char _binary___bin_ps_bin_start[], _binary___bin_ps_bin_size[];
extern char _binary___bin_ipc_rx_bin_start[], _binary___bin_ipc_rx_bin_size[];

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
    if (index < 0 || index >= PROCS_MAX) {
        f->a0 = -1;
        return;
    }

    struct process *proc = &procs[index];
    f->a0 = ((proc->state & 0xffff) << 16) | (proc->pid & 0xffff);
}

void syscall_handle_clone(struct trap_frame *f) {
    const void *image = NULL;
    size_t image_size = 0;

    switch ((int) f->a0) {
        case APP_ID_SHELL:
            image = _binary___bin_shell_bin_start;
            image_size = (size_t) _binary___bin_shell_bin_size;
            break;
        case APP_ID_PS:
            image = _binary___bin_ps_bin_start;
            image_size = (size_t) _binary___bin_ps_bin_size;
            break;
        case APP_ID_IPC_RX:
            image = _binary___bin_ipc_rx_bin_start;
            image_size = (size_t) _binary___bin_ipc_rx_bin_size;
            break;
        default:
            f->a0 = -1;
            return;
    }

    struct process *proc = create_process(image, image_size);
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
