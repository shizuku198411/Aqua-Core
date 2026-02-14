#include "syscall_internal.h"
#include "syscall.h"
#include "user_apps.h"
#include "process.h"
#include "kernel.h"
#include "commonlibs.h"

extern struct process *current_proc;
extern struct process *init_proc;

// apps address
extern char _binary___bin_shell_bin_start[], _binary___bin_shell_bin_size[];        // shell
extern char _binary___bin_ipc_rx_bin_start[], _binary___bin_ipc_rx_bin_size[];      // ipc_rx
extern char _binary___bin_ps_bin_start[], _binary___bin_ps_bin_size[];              // ps
extern char _binary___bin_date_bin_start[], _binary___bin_date_bin_size[];          // date
extern char _binary___bin_ls_bin_start[], _binary___bin_ls_bin_size[];              // ls
extern char _binary___bin_mkdir_bin_start[], _binary___bin_mkdir_bin_size[];        // mkdir
extern char _binary___bin_rmdir_bin_start[], _binary___bin_rmdir_bin_size[];        // rmdir
extern char _binary___bin_touch_bin_start[], _binary___bin_touch_bin_size[];        // touch
extern char _binary___bin_rm_bin_start[], _binary___bin_rm_bin_size[];              // rm
extern char _binary___bin_write_bin_start[], _binary___bin_write_bin_size[];        // write
extern char _binary___bin_cat_bin_start[], _binary___bin_cat_bin_size[];            // cat
extern char _binary___bin_kill_bin_start[], _binary___bin_kill_bin_size[];          // kill
extern char _binary___bin_kernel_info_bin_start[], _binary___bin_kernel_info_bin_size[]; // kernel_info
extern char _binary___bin_bitmap_bin_start[], _binary___bin_bitmap_bin_size[];      // bitmap

static int resolve_app_image(int app_id, const void **image_out, size_t *size_out, const char **name_out) {
    if (!image_out || !size_out || !name_out) {
        return -1;
    }

    switch (app_id) {
        case APP_ID_SHELL:
            *image_out = _binary___bin_shell_bin_start;
            *size_out = (size_t) _binary___bin_shell_bin_size;
            *name_out = APP_NAME_SHELL;
            return 0;
        case APP_ID_IPC_RX:
            *image_out = _binary___bin_ipc_rx_bin_start;
            *size_out = (size_t) _binary___bin_ipc_rx_bin_size;
            *name_out = APP_NAME_IPC_RX;
            return 0;
        case APP_ID_PS:
            *image_out = _binary___bin_ps_bin_start;
            *size_out = (size_t) _binary___bin_ps_bin_size;
            *name_out = APP_NAME_PS;
            return 0;
        case APP_ID_DATE:
            *image_out = _binary___bin_date_bin_start;
            *size_out = (size_t) _binary___bin_date_bin_size;
            *name_out = APP_NAME_DATE;
            return 0;
        case APP_ID_LS:
            *image_out = _binary___bin_ls_bin_start;
            *size_out = (size_t) _binary___bin_ls_bin_size;
            *name_out = APP_NAME_LS;
            return 0;
        case APP_ID_MKDIR:
            *image_out = _binary___bin_mkdir_bin_start;
            *size_out = (size_t) _binary___bin_mkdir_bin_size;
            *name_out = APP_NAME_MKDIR;
            return 0;
        case APP_ID_RMDIR:
            *image_out = _binary___bin_rmdir_bin_start;
            *size_out = (size_t) _binary___bin_rmdir_bin_size;
            *name_out = APP_NAME_RMDIR;
            return 0;
        case APP_ID_TOUCH:
            *image_out = _binary___bin_touch_bin_start;
            *size_out = (size_t) _binary___bin_touch_bin_size;
            *name_out = APP_NAME_TOUCH;
            return 0;
        case APP_ID_RM:
            *image_out = _binary___bin_rm_bin_start;
            *size_out = (size_t) _binary___bin_rm_bin_size;
            *name_out = APP_NAME_RM;
            return 0;
        case APP_ID_WRITE:
            *image_out = _binary___bin_write_bin_start;
            *size_out = (size_t) _binary___bin_write_bin_size;
            *name_out = APP_NAME_WRITE;
            return 0;
        case APP_ID_CAT:
            *image_out = _binary___bin_cat_bin_start;
            *size_out = (size_t) _binary___bin_cat_bin_size;
            *name_out = APP_NAME_CAT;
            return 0;
        case APP_ID_KILL:
            *image_out = _binary___bin_kill_bin_start;
            *size_out = (size_t) _binary___bin_kill_bin_size;
            *name_out = APP_NAME_KILL;
            return 0;
        case APP_ID_KERNEL_INFO:
            *image_out = _binary___bin_kernel_info_bin_start;
            *size_out = (size_t) _binary___bin_kernel_info_bin_size;
            *name_out = APP_NAME_KERNEL_INFO;
            return 0;
        case APP_ID_BITMAP:
            *image_out = _binary___bin_bitmap_bin_start;
            *size_out = (size_t) _binary___bin_bitmap_bin_size;
            *name_out = APP_NAME_BITMAP;
            return 0;
        default:
            return -1;
    }
}


static int copy_user_argv(const char *const *uargv,
                          int *argc_out,
                          char out_argv[PROC_EXEC_ARGV_MAX][PROC_EXEC_ARG_LEN]) {
    if (!argc_out || !out_argv) {
        return -1;
    }

    *argc_out = 0;
    for (int i = 0; i < PROC_EXEC_ARGV_MAX; i++) {
        for (int j = 0; j < PROC_EXEC_ARG_LEN; j++) {
            out_argv[i][j] = '\0';
        }
    }

    if (!uargv) {
        return 0;
    }

    int argc = 0;
    for (; argc < PROC_EXEC_ARGV_MAX; argc++) {
        const char *p = uargv[argc];
        if (!p) {
            break;
        }

        for (int j = 0; j < PROC_EXEC_ARG_LEN - 1; j++) {
            char c = p[j];
            out_argv[argc][j] = c;
            if (c == '\0') {
                break;
            }
        }
        out_argv[argc][PROC_EXEC_ARG_LEN - 1] = '\0';
    }

    *argc_out = argc;
    return 0;
}


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
        current_proc->wait_reason = PROC_WAIT_NONE;
        current_proc->wait_pid = -1;
        if (procfs_sync_process(current_proc) < 0) {
            printf("procfs sync failed\n");
        }
        kernel_shutdown();
    }

    orphan_children(current_proc->pid);
    current_proc->state = PROC_EXITED;
    current_proc->wait_reason = PROC_WAIT_NONE;
    current_proc->wait_pid = -1;
    if (procfs_sync_process(current_proc) < 0) {
        printf("procfs sync failed\n");
    }
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
    if (resolve_app_image((int) f->a0, &image, &image_size, &name) < 0) {
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

void syscall_handle_fork(struct trap_frame *f) {
    int child_pid = process_fork(f);
    if (child_pid < 0) {
        f->a0 = -1;
        return;
    }
    f->a0 = child_pid;
}

void syscall_handle_exec(struct trap_frame *f) {
    const void *image = NULL;
    size_t image_size = 0;
    const char *name = NULL;
    if (resolve_app_image((int) f->a0, &image, &image_size, &name) < 0) {
        f->a0 = -1;
        return;
    }

    int ret = process_exec(image, image_size, name, 0, NULL);
    f->a0 = (ret < 0) ? -1 : 0;
}

void syscall_handle_execv(struct trap_frame *f) {
    const void *image = NULL;
    size_t image_size = 0;
    const char *name = NULL;
    if (resolve_app_image((int) f->a0, &image, &image_size, &name) < 0) {
        f->a0 = -1;
        return;
    }

    int argc = 0;
    char argv[PROC_EXEC_ARGV_MAX][PROC_EXEC_ARG_LEN];
    uint32_t sstatus = READ_CSR(sstatus);
    WRITE_CSR(sstatus, sstatus | SSTATUS_SUM);
    int cret = copy_user_argv((const char *const *) f->a1, &argc, argv);
    WRITE_CSR(sstatus, sstatus);
    if (cret < 0) {
        f->a0 = -1;
        return;
    }

    int ret = process_exec(image, image_size, name, argc, argv);
    f->a0 = (ret < 0) ? -1 : 0;
}

void syscall_handle_getargs(struct trap_frame *f) {
    struct exec_args *out = (struct exec_args *) f->a0;
    if (!out || !current_proc) {
        f->a0 = -1;
        return;
    }

    uint32_t sstatus = READ_CSR(sstatus);
    WRITE_CSR(sstatus, sstatus | SSTATUS_SUM);
    out->argc = current_proc->exec_argc;
    for (int i = 0; i < PROC_EXEC_ARGV_MAX; i++) {
        for (int j = 0; j < PROC_EXEC_ARG_LEN; j++) {
            out->argv[i][j] = current_proc->exec_argv[i][j];
        }
    }
    WRITE_CSR(sstatus, sstatus);
    f->a0 = 0;
}
