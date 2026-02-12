#include "syscall_internal.h"
#include "syscall.h"
#include "process.h"

extern struct process *current_proc;
extern char _binary___bin_shell_bin_start[], _binary___bin_shell_bin_size[];
extern char _binary___bin_ps_bin_start[], _binary___bin_ps_bin_size[];

void syscall_handle_exit(struct trap_frame *f) {
    (void) f;
    current_proc->state = PROC_EXITED;
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
        default:
            f->a0 = -1;
            return;
    }

    struct process *proc = create_process(image, image_size);
    f->a0 = proc->pid;
}
