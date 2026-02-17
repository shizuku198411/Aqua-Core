#include "syscall_internal.h"
#include "kernel/kernel.h"
#include "proc/process.h"
#include "fs/internal.h"
#include "core/commonlibs.h"
#include "kernel/page_access.h"

extern struct process *current_proc;

#define KERNEL_BUFF_SIZE 256

void syscall_handle_open(struct trap_frame *f) {
    if (!current_proc) {
        f->a0 = -1;
        return;
    }

    char path[FS_PATH_MAX];
    if (copyinstr(path, (const char *) f->a0, FS_PATH_MAX) < 0) {
        f->a0 = -1;
        return;
    }
    int flags = (int) f->a1;

    int ret = fs_open(current_proc->pid, path, flags);

    f->a0 = ret;
}

void syscall_handle_close(struct trap_frame *f) {
    if (!current_proc) {
        f->a0 = -1;
        return;
    }

    f->a0 = fs_close(current_proc->pid, (int) f->a0);
}

void syscall_handle_read(struct trap_frame *f) {
    if (!current_proc) {
        f->a0 = -1;
        return;
    }

    int fd = (int) f->a0;
    void *user_buf = (void *) f->a1;
    size_t req = (size_t) f->a2;

    if (!user_buf && req > 0) {
        f->a0 = -1;
        return;
    }
    if (req == 0) {
        f->a0 = 0;
        return;
    }
    
    uint8_t kbuf[KERNEL_BUFF_SIZE];
    size_t done = 0;
    
    while (done < req) {
        size_t chunk = req - done;
        if (chunk > sizeof(kbuf)) {
            chunk = sizeof(kbuf);
        }
        int n = fs_read(current_proc->pid, fd, kbuf, chunk);
        if (n < 0) {
            f->a0 = (done > 0) ? (int) done : -1;
            return;
        }
        if (n == 0) {
            f->a0 = (int) done;
            return;
        }
        if (copyout((uint8_t *) user_buf + done, kbuf, (size_t) n) < 0) {
            f->a0 = (done > 0) ? (int) done : -1;
            return;
        }
        done += (size_t) n;
        if ((size_t) n < chunk) {
            break;
        }
    }

    f->a0 = (int) done;
}

void syscall_handle_write(struct trap_frame *f) {
    if (!current_proc) {
        f->a0 = -1;
        return;
    }

    int fd = (int) f->a0;
    const void *user_buf = (const void *) f->a1;
    size_t req = (size_t) f->a2;

    if (!user_buf && req > 0) {
        f->a0 = -1;
        return;
    }
    if (req == 0) {
        f->a0 = 0;
        return;
    }

    uint8_t kbuf[KERNEL_BUFF_SIZE];
    size_t done = 0;

    while (done < req) {
        size_t chunk = req - done;
        if (chunk > sizeof(kbuf)) {
            chunk = sizeof(kbuf);
        }

        if (copyin(kbuf, (const uint8_t *) user_buf + done, chunk) < 0) {
            f->a0 = (done > 0) ? (int) done : -1;
            return;
        }
        int n = fs_write(current_proc->pid, fd, kbuf, chunk);
        if (n < 0) {
            f->a0 = (done > 0) ? (int) done : -1;
            return;
        }
        if (n == 0) {
            f->a0 = (int) done;
            return;
        }
        done += (size_t) n;
        if ((size_t) n < chunk) {
            break;
        }
    }

    f->a0 = (int) done;
}

void syscall_handle_mkdir(struct trap_frame *f) {
    char path[FS_PATH_MAX];
    if (copyinstr(path, (const char *) f->a0, FS_PATH_MAX) < 0) {
        f->a0 = -1;
        return;
    }

    int ret = fs_mkdir(path);

    f->a0 = ret;
}

void syscall_handle_readdir(struct trap_frame *f) {
    char path[FS_PATH_MAX];
    if (copyinstr(path, (const char *) f->a0, FS_PATH_MAX) < 0) {
        f->a0 = -1;
        return;
    }
    int index = (int) f->a1;
    struct fs_dirent *user_buf = (struct fs_dirent *) f->a2;
    if (!user_buf) {
        f->a0 = -1;
        return;
    }

    struct fs_dirent ent;
    if (fs_readdir(path, index, &ent) < 0) {
        f->a0 = -1;
        return;
    }
    if (copyout(user_buf, &ent, sizeof(ent)) < 0) {
        f->a0 = -1;
        return;
    }

    f->a0 = 0;
}

void syscall_handle_unlink(struct trap_frame *f) {
    char path[FS_PATH_MAX];
    if (copyinstr(path, (const char *) f->a0, FS_PATH_MAX) < 0) {
        f->a0 = -1;
        return;
    }

    int ret = fs_unlink(path);

    f->a0 = ret;
}

void syscall_handle_rmdir(struct trap_frame *f) {
    char path[FS_PATH_MAX];
    if (copyinstr(path, (const char *) f->a0, FS_PATH_MAX) < 0) {
        f->a0 = -1;
        return;
    }

    int ret = fs_rmdir(path);

    f->a0 = ret;
}

void syscall_handle_dup2(struct trap_frame *f) {
    int fd1 = f->a0;
    int fd2 = f->a1;

    int ret = fs_dup2(current_proc->pid, fd1, fd2);
    if (ret < 0) {
        f->a0 = -1;
        return;
    }
    f->a0 = ret;
}

void syscall_handle_getcwd(struct trap_frame *f) {
    if (!current_proc) {
        f->a0 = -1;
        return;
    }
    char *cwd_path = (char *) f->a0;
    if (!cwd_path) {
        f->a0 = -1;
        return;
    }

    if (copyout(cwd_path, current_proc->cwd_path, FS_PATH_MAX) < 0) {
        f->a0 = -1;
        return;
    }

    f->a0 = 0;
}

void syscall_handle_chdir(struct trap_frame *f) {
    if (!current_proc) {
        f->a0 = -1;
        return;
    }

    char path[FS_PATH_MAX];
    if (copyinstr(path, (const char *) f->a0, FS_PATH_MAX) < 0) {
        f->a0 = -1;
        return;
    }

    int mount_idx, node_idx;
    if (fs_get_path_entry(&mount_idx, &node_idx, path) < 0) {
        f->a0 = -1;
        return;
    }

    current_proc->cwd_mount_idx = mount_idx;
    current_proc->cwd_node_idx = node_idx;
    strcpy_s(current_proc->cwd_path, FS_PATH_MAX, path);

    // sync procfs
    if (procfs_sync_process(current_proc) < 0) {
        printf("procfs sync failed\n");
    }

    f->a0 = 0;
}
