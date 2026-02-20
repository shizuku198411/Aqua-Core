#include "syscall_internal.h"
#include "user/syscall.h"
#include "proc/process.h"
#include "kernel/kernel.h"
#include "core/commonlibs.h"
#include "kernel/page_access.h"
#include "fs/internal.h"
#include "mm/memory.h"

extern struct process *current_proc;
extern struct process *init_proc;

#define EXEC_PATH_MAX       FS_PATH_MAX
#define EXEC_IMAGE_MAX_SIZE (128 * 1024)

static int basename_from_path(const char *path, char out_name[PROC_NAME_MAX]) {
    if (!path || !out_name || path[0] == '\0') {
        return -1;
    }

    int last = 0;
    for (int i = 0; path[i] != '\0'; i++) {
        if (path[i] == '/') {
            last = i + 1;
        }
    }
    if (path[last] == '\0') {
        return -1;
    }
    strcpy_s(out_name, PROC_NAME_MAX, path + last);
    return 0;
}

static int load_exec_image_from_path(const char *path,
                                     void **image_out,
                                     size_t *size_out,
                                     uint32_t *pages_out) {
    if (!path || !image_out || !size_out || !pages_out || !current_proc) {
        return -1;
    }

    int fd = fs_open(current_proc->pid, path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    uint32_t pages = (EXEC_IMAGE_MAX_SIZE + PAGE_SIZE - 1) / PAGE_SIZE;
    void *buf = (void *) alloc_pages(pages);
    if (!buf) {
        (void) fs_close(current_proc->pid, fd);
        return -1;
    }

    size_t total = 0;
    int ret = -1;
    while (1) {
        if (total >= EXEC_IMAGE_MAX_SIZE) {
            goto out;
        }
        size_t remain = EXEC_IMAGE_MAX_SIZE - total;
        int n = fs_read(current_proc->pid, fd, (uint8_t *) buf + total, remain);
        if (n < 0) {
            goto out;
        }
        if (n == 0) {
            break;
        }
        total += (size_t) n;
    }
    if (total == 0) {
        goto out;
    }

    *image_out = buf;
    *size_out = total;
    *pages_out = pages;
    ret = 0;

out:
    (void) fs_close(current_proc->pid, fd);
    if (ret < 0) {
        free_pages((paddr_t) buf, pages);
    }
    return ret;
}

static int exec_from_path(const char *path,
                          int argc,
                          const char argv[PROC_EXEC_ARGV_MAX][PROC_EXEC_ARG_LEN]) {
    if (!path) {
        return -1;
    }

    char proc_name[PROC_NAME_MAX];
    if (basename_from_path(path, proc_name) < 0) {
        return -1;
    }

    void *image = NULL;
    size_t image_size = 0;
    uint32_t pages = 0;
    if (load_exec_image_from_path(path, &image, &image_size, &pages) < 0) {
        return -1;
    }

    int ret = process_exec(image, image_size, proc_name, argc, argv);
    free_pages((paddr_t) image, pages);
    return ret;
}

static int copy_user_path_safe(const char *user_path, char out_path[EXEC_PATH_MAX]) {
    if (!user_path || !out_path) {
        return -1;
    }
    if (copyinstr(out_path, user_path, EXEC_PATH_MAX) < 0) {
        return -1;
    }
    if (out_path[0] != '/') {
        return -1;
    }
    return 0;
}


static int write_user_ps_info(struct ps_info *user_ptr, const struct process *proc) {
    if (!user_ptr || !proc) {
        return -1;
    }

    struct ps_info kbuf;
    memset(&kbuf, 0, sizeof(kbuf));
    kbuf.pid = proc->pid;
    kbuf.parent_pid = proc->parent_pid;
    kbuf.state = proc->state;
    kbuf.wait_reason = proc->wait_reason;
    kbuf.exit_code = proc->exit_code;
    strcpy_s(kbuf.name, sizeof(kbuf.name), proc->name);

    int argc = proc->exec_argc;
    if (argc < 0 || argc > PROC_EXEC_ARGV_MAX) {
        argc = 0;
    }
    kbuf.argc = argc;
    for (int i = 0; i < PROC_EXEC_ARGV_MAX; i++) {
        strcpy_s(kbuf.argv[i], sizeof(kbuf.argv[i]), proc->exec_argv[i]);
        kbuf.argv[i][PROC_EXEC_ARG_LEN - 1] = '\0';
    }
    kbuf.name[PROC_NAME_MAX - 1] = '\0';

    if (copyout(user_ptr, &kbuf, sizeof(kbuf)) < 0) {
        return -1;
    }
    return 0;
}

void syscall_handle_exit(struct trap_frame *f) {
    int status = (int) f->a0;

    // Treat pid=1 as init process. When init exits, shut down kernel.
    if (current_proc && current_proc == init_proc) {
        current_proc->exit_code = status;
        current_proc->state = PROC_EXITED;
        current_proc->wait_reason = PROC_WAIT_NONE;
        current_proc->wait_pid = -1;
        if (procfs_sync_process(current_proc) < 0) {
            printf("procfs sync failed\n");
        }
        kernel_shutdown();
    }

    orphan_children(current_proc->pid);
    current_proc->exit_code = status;
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
    if (!current_proc || index < 0 || index >= PROCS_MAX) {
        f->a0 = -1;
        return;
    }

    // ps() writes into caller-owned output buffer. In this codebase, caller uses
    // a stack local buffer, so restrict to stack area to avoid accidental text overwrite
    // when a corrupted pointer is passed.
    uintptr_t user_top = USER_BASE + (uintptr_t) current_proc->user_pages * PAGE_SIZE;
    uintptr_t stack_floor = (user_top > (uintptr_t) (64 * 1024))
                          ? (user_top - (uintptr_t) (64 * 1024))
                          : USER_BASE;
    uintptr_t ptr = (uintptr_t) info_ptr;
    if (!info_ptr ||
        ptr < stack_floor ||
        ptr + sizeof(struct ps_info) > user_top ||
        ptr + sizeof(struct ps_info) < ptr) {
        f->a0 = -1;
        return;
    }

    struct process *proc = &procs[index];
    if (write_user_ps_info(info_ptr, proc) < 0) {
        f->a0 = -1;
        return;
    }
    f->a0 = 0;
}

void syscall_handle_waitpid(struct trap_frame *f) {
    int target_pid = (int) f->a0;
    int *status_ptr = (int *) f->a1;
    int options = (int) f->a2;
    if (target_pid <= 0 && target_pid != -1) {
        f->a0 = -1;
        return;
    }
    if ((options & ~WAITPID_NOHANG) != 0) {
        f->a0 = -1;
        return;
    }

    int exit_status = 0;
    int waited_pid = wait_for_child_exit(current_proc->pid, target_pid, options, &exit_status);
    if (waited_pid > 0 && status_ptr) {
        if (copyout(status_ptr, &exit_status, sizeof(exit_status)) < 0) {
            f->a0 = -1;
            return;
        }
    }
    f->a0 = waited_pid;
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

static int copy_user_argv_safe(const char *const *uargv,
                               int *argc_out,
                               char out_argv[PROC_EXEC_ARGV_MAX][PROC_EXEC_ARG_LEN]) {
    if (!argc_out || !out_argv) return -1;

    *argc_out = 0;
    for (int i = 0; i < PROC_EXEC_ARGV_MAX; i++) {
        for (int j = 0; j < PROC_EXEC_ARG_LEN; j++) {
            out_argv[i][j] = '\0';
        }
    }

    if (!uargv) return 0; //

    for (int i = 0; i < PROC_EXEC_ARGV_MAX; i++) {
        const char *uptr = NULL;

        if (copyin(&uptr, (const uint8_t *)uargv + i * sizeof(uptr), sizeof(uptr)) < 0) {
            return -1;
        }

        if (!uptr) {
            *argc_out = i;
            return 0;
        }

        if (copyinstr(out_argv[i], uptr, PROC_EXEC_ARG_LEN) < 0) {
            return -1;
        }
    }

    *argc_out = PROC_EXEC_ARGV_MAX;
    return 0;
}

void syscall_handle_exec_path(struct trap_frame *f) {
    char path[EXEC_PATH_MAX];
    if (copy_user_path_safe((const char *) f->a0, path) < 0) {
        f->a0 = -1;
        return;
    }

    int ret = exec_from_path(path, 0, NULL);
    f->a0 = (ret < 0) ? -1 : 0;
}

void syscall_handle_execv_path(struct trap_frame *f) {
    char path[EXEC_PATH_MAX];
    if (copy_user_path_safe((const char *) f->a0, path) < 0) {
        f->a0 = -1;
        return;
    }

    int argc = 0;
    char argv[PROC_EXEC_ARGV_MAX][PROC_EXEC_ARG_LEN];
    if (copy_user_argv_safe((const char *const *) f->a1, &argc, argv) < 0) {
        f->a0 = -1;
        return;
    }

    int ret = exec_from_path(path, argc, argv);
    f->a0 = (ret < 0) ? -1 : 0;
}

void syscall_handle_getargs(struct trap_frame *f) {
    struct exec_args *out = (struct exec_args *) f->a0;
    if (!out || !current_proc) {
        f->a0 = -1;
        return;
    }

    if (copyout(&out->argc, &current_proc->exec_argc, sizeof(out->argc)) < 0) {
        f->a0 = -1;
        return;
    }
    for (int i = 0; i < PROC_EXEC_ARGV_MAX; i++) {
        for (int j = 0; j < PROC_EXEC_ARG_LEN; j++) {
            if (copyout(&out->argv[i][j], &current_proc->exec_argv[i][j], sizeof(out->argv[i][j])) < 0) {
                f->a0 = -1;
                return;
            }
        }
    }

    f->a0 = 0;
}
