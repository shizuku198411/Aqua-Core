#include "syscall_internal.h"
#include "kernel.h"
#include "process.h"
#include "fs_internal.h"
#include "commonlibs.h"

#define SSTATUS_SUM (1u << 18)

extern struct process *current_proc;

void syscall_handle_open(struct trap_frame *f) {
    if (!current_proc) {
        f->a0 = -1;
        return;
    }

    const char *path = (const char *) f->a0;
    int flags = (int) f->a1;

    if (!path) {
        f->a0 = -1;
        return;
    }

    uint32_t sstatus = READ_CSR(sstatus);
    WRITE_CSR(sstatus, sstatus | SSTATUS_SUM);
    int ret = fs_open(current_proc->pid, path, flags);
    WRITE_CSR(sstatus, sstatus);

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
    void *buf = (void *) f->a1;
    size_t size = (size_t) f->a2;

    if (!buf && size > 0) {
        f->a0 = -1;
        return;
    }

    uint32_t sstatus = READ_CSR(sstatus);
    WRITE_CSR(sstatus, sstatus | SSTATUS_SUM);
    int ret = fs_read(current_proc->pid, fd, buf, size);
    WRITE_CSR(sstatus, sstatus);

    f->a0 = ret;
}

void syscall_handle_write(struct trap_frame *f) {
    if (!current_proc) {
        f->a0 = -1;
        return;
    }

    int fd = (int) f->a0;
    const void *buf = (const void *) f->a1;
    size_t size = (size_t) f->a2;

    if (!buf && size > 0) {
        f->a0 = -1;
        return;
    }

    uint32_t sstatus = READ_CSR(sstatus);
    WRITE_CSR(sstatus, sstatus | SSTATUS_SUM);
    int ret = fs_write(current_proc->pid, fd, buf, size);
    WRITE_CSR(sstatus, sstatus);

    f->a0 = ret;
}

void syscall_handle_mkdir(struct trap_frame *f) {
    const char *path = (const char *) f->a0;
    if (!path) {
        f->a0 = -1;
        return;
    }

    uint32_t sstatus = READ_CSR(sstatus);
    WRITE_CSR(sstatus, sstatus | SSTATUS_SUM);
    int ret = fs_mkdir(path);
    WRITE_CSR(sstatus, sstatus);

    f->a0 = ret;
}

void syscall_handle_readdir(struct trap_frame *f) {
    const char *path = (const char *) f->a0;
    int index = (int) f->a1;
    struct fs_dirent *out = (struct fs_dirent *) f->a2;

    if (!path || !out) {
        f->a0 = -1;
        return;
    }

    uint32_t sstatus = READ_CSR(sstatus);
    WRITE_CSR(sstatus, sstatus | SSTATUS_SUM);
    int ret = fs_readdir(path, index, out);
    WRITE_CSR(sstatus, sstatus);

    f->a0 = ret;
}

void syscall_handle_unlink(struct trap_frame *f) {
    const char *path = (const char *) f->a0;
    if (!path) {
        f->a0 = -1;
        return;
    }

    uint32_t sstatus = READ_CSR(sstatus);
    WRITE_CSR(sstatus, sstatus | SSTATUS_SUM);
    int ret = fs_unlink(path);
    WRITE_CSR(sstatus, sstatus);

    f->a0 = ret;
}

void syscall_handle_rmdir(struct trap_frame *f) {
    const char *path = (const char *) f->a0;
    if (!path) {
        f->a0 = -1;
        return;
    }

    uint32_t sstatus = READ_CSR(sstatus);
    WRITE_CSR(sstatus, sstatus | SSTATUS_SUM);
    int ret = fs_rmdir(path);
    WRITE_CSR(sstatus, sstatus);

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

    uint32_t sstatus = READ_CSR(sstatus);
    WRITE_CSR(sstatus, sstatus | SSTATUS_SUM);
    strcpy_s(cwd_path, FS_PATH_MAX, current_proc->cwd_path);
    WRITE_CSR(sstatus, sstatus);

    f->a0 = 0;
}

void syscall_handle_chdir(struct trap_frame *f) {
    if (!current_proc) {
        f->a0 = -1;
        return;
    }

    const char *user_path = (const char *) f->a0;
    if (!user_path) {
        f->a0 = -1;
        return;
    }

    char path[FS_PATH_MAX];
    uint32_t sstatus = READ_CSR(sstatus);
    WRITE_CSR(sstatus, sstatus | SSTATUS_SUM);
    strcpy_s(path, sizeof(path), user_path);
    WRITE_CSR(sstatus, sstatus);

    int mount_idx, node_idx;
    if (fs_get_path_entry(&mount_idx, &node_idx, path) < 0) {
        f->a0 = -1;
        return;
    }

    current_proc->cwd_mount_idx = mount_idx;
    current_proc->cwd_node_idx = node_idx;
    strcpy_s(current_proc->cwd_path, FS_PATH_MAX, path);

    f->a0 = 0;
}
