#include "fs_private.h"
#include "proc/process.h"
#include "core/commonlibs.h"

#define PROCFS_NODE_STATUS_BASE 1000

static const char *proc_state_str_local(int state) {
    switch (state) {
        case PROC_UNUSED: return "UNUSED";
        case PROC_RUNNABLE: return "RUN";
        case PROC_WAITTING: return "WAIT";
        case PROC_EXITED: return "EXIT";
        default: return "UNKNOWN";
    }
}

static const char *proc_wait_reason_str_local(int wait_reason) {
    switch (wait_reason) {
        case PROC_WAIT_NONE: return "NONE";
        case PROC_WAIT_CONSOLE_INPUT: return "CONSOLE_INPUT";
        case PROC_WAIT_CHILD_EXIT: return "CHILD_EXIT";
        case PROC_WAIT_IPC_RECV: return "IPC_RECV";
        case PROC_WAIT_SLEEP: return "SLEEP";
        default: return "UNKNOWN";
    }
}

static int append_char_k(char *out, size_t out_size, size_t *pos, char c) {
    if (*pos + 1 >= out_size) {
        return -1;
    }
    out[*pos] = c;
    (*pos)++;
    out[*pos] = '\0';
    return 0;
}

static int append_str_k(char *out, size_t out_size, size_t *pos, const char *s) {
    while (*s) {
        if (append_char_k(out, out_size, pos, *s++) < 0) {
            return -1;
        }
    }
    return 0;
}

static int append_u32_k(char *out, size_t out_size, size_t *pos, uint32_t v) {
    char tmp[10];
    int n = 0;
    if (v == 0) {
        return append_char_k(out, out_size, pos, '0');
    }
    while (v > 0 && n < (int) sizeof(tmp)) {
        tmp[n++] = (char) ('0' + (v % 10));
        v /= 10;
    }
    for (int i = n - 1; i >= 0; i--) {
        if (append_char_k(out, out_size, pos, tmp[i]) < 0) {
            return -1;
        }
    }
    return 0;
}

static int append_key_val_u32(char *out, size_t out_size, size_t *pos,
                              const char *key, uint32_t value) {
    if (append_str_k(out, out_size, pos, key) < 0) return -1;
    if (append_str_k(out, out_size, pos, ":\t") < 0) return -1;
    if (append_u32_k(out, out_size, pos, value) < 0) return -1;
    if (append_char_k(out, out_size, pos, '\n') < 0) return -1;
    return 0;
}

static int append_key_val_str(char *out, size_t out_size, size_t *pos,
                              const char *key, const char *value) {
    if (append_str_k(out, out_size, pos, key) < 0) return -1;
    if (append_str_k(out, out_size, pos, ":\t") < 0) return -1;
    if (append_str_k(out, out_size, pos, value) < 0) return -1;
    if (append_char_k(out, out_size, pos, '\n') < 0) return -1;
    return 0;
}

static int parse_pid_component(const char *s, int *pid_out, const char **rest_out) {
    if (!s || s[0] != '/' || !pid_out || !rest_out) {
        return -1;
    }
    int i = 1;
    if (s[i] < '0' || s[i] > '9') {
        return -1;
    }
    uint32_t pid = 0;
    while (s[i] >= '0' && s[i] <= '9') {
        pid = pid * 10 + (uint32_t) (s[i] - '0');
        i++;
    }
    *pid_out = (int) pid;
    *rest_out = s + i;
    return 0;
}

static const struct process *find_proc_by_pid(int pid) {
    if (pid <= 0) {
        return NULL;
    }
    for (int i = 0; i < PROCS_MAX; i++) {
        if (procs[i].pid == pid && procs[i].state != PROC_UNUSED) {
            return &procs[i];
        }
    }
    return NULL;
}

static int procfs_open(void *ctx, const char *path, int flags, int *node_out, uint32_t *offset_out) {
    (void) ctx;
    if (!path || !node_out || !offset_out) {
        return -1;
    }

    if ((flags & O_WRONLY) || (flags & O_CREAT) || (flags & O_TRUNC)) {
        return -1;
    }

    int pid = 0;
    const char *rest = NULL;
    if (parse_pid_component(path, &pid, &rest) < 0) {
        return -1;
    }
    if (strcmp(rest, "/status") != 0) {
        return -1;
    }
    if (!find_proc_by_pid(pid)) {
        return -1;
    }

    *node_out = PROCFS_NODE_STATUS_BASE + pid;
    *offset_out = 0;
    return 0;
}

static int procfs_build_status(const struct process *proc, char *out, size_t out_size, size_t *len_out) {
    if (!proc || !out || !len_out) {
        return -1;
    }
    size_t pos = 0;
    out[0] = '\0';

    if (append_key_val_u32(out, out_size, &pos, "pid", (uint32_t) proc->pid) < 0) return -1;
    if (append_key_val_u32(out, out_size, &pos, "ppid", (uint32_t) proc->parent_pid) < 0) return -1;
    if (append_key_val_str(out, out_size, &pos, "name", proc->name) < 0) return -1;
    if (append_key_val_u32(out, out_size, &pos, "state_id", (uint32_t) proc->state) < 0) return -1;
    if (append_key_val_str(out, out_size, &pos, "state", proc_state_str_local(proc->state)) < 0) return -1;
    if (append_key_val_u32(out, out_size, &pos, "wait_reason_id", (uint32_t) proc->wait_reason) < 0) return -1;
    if (append_key_val_str(out, out_size, &pos, "wait_reason", proc_wait_reason_str_local(proc->wait_reason)) < 0) return -1;
    if (append_key_val_u32(out, out_size, &pos, "exit_code", (uint32_t) proc->exit_code) < 0) return -1;
    if (append_key_val_str(out, out_size, &pos, "cwd", proc->cwd_path) < 0) return -1;

    *len_out = pos;
    return 0;
}

static int procfs_read(void *ctx, int node, uint32_t *offset, void *buf, size_t size) {
    (void) ctx;
    if (!offset || !buf) {
        return -1;
    }

    if (node < PROCFS_NODE_STATUS_BASE) {
        return -1;
    }
    int pid = node - PROCFS_NODE_STATUS_BASE;
    const struct process *proc = find_proc_by_pid(pid);
    if (!proc) {
        return -1;
    }

    char content[384];
    size_t len = 0;
    if (procfs_build_status(proc, content, sizeof(content), &len) < 0) {
        return -1;
    }

    if (*offset >= len) {
        return 0;
    }
    size_t remain = len - *offset;
    size_t n = (size < remain) ? size : remain;
    memcpy(buf, content + *offset, n);
    *offset += (uint32_t) n;
    return (int) n;
}

static int procfs_mkdir(void *ctx, const char *path) {
    (void) ctx;
    (void) path;
    return -1;
}

static int procfs_write(void *ctx, int node, uint32_t *offset, const void *buf, size_t size) {
    (void) ctx;
    (void) node;
    (void) offset;
    (void) buf;
    (void) size;
    return -1;
}

static int procfs_readdir(void *ctx, const char *path, int index, struct fs_dirent *out) {
    (void) ctx;
    if (!path || index < 0 || !out) {
        return -1;
    }

    if (strcmp(path, "/") == 0) {
        int seen = 0;
        for (int i = 0; i < PROCS_MAX; i++) {
            if (procs[i].pid <= 0 || procs[i].state == PROC_UNUSED) {
                continue;
            }
            if (seen == index) {
                memset(out, 0, sizeof(*out));
                size_t pos = 0;
                if (append_u32_k(out->name, FS_NAME_MAX, &pos, (uint32_t) procs[i].pid) < 0) {
                    return -1;
                }
                out->type = FS_TYPE_DIR;
                out->size = 0;
                return 0;
            }
            seen++;
        }
        return -1;
    }

    int pid = 0;
    const char *rest = NULL;
    if (parse_pid_component(path, &pid, &rest) < 0) {
        return -1;
    }
    if (rest[0] != '\0') {
        return -1;
    }
    if (!find_proc_by_pid(pid)) {
        return -1;
    }

    if (index != 0) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    strcpy_s(out->name, FS_NAME_MAX, "status");
    out->type = FS_TYPE_FILE;
    out->size = 0;
    return 0;
}

static int procfs_unlink(void *ctx, const char *path) {
    (void) ctx;
    (void) path;
    return -1;
}

static int procfs_rmdir(void *ctx, const char *path) {
    (void) ctx;
    (void) path;
    return -1;
}

const struct vfs_ops procfs_ops = {
    .open = procfs_open,
    .read = procfs_read,
    .write = procfs_write,
    .mkdir = procfs_mkdir,
    .readdir = procfs_readdir,
    .unlink = procfs_unlink,
    .rmdir = procfs_rmdir,
};

void procfs_init_instance(struct nodefs *fs) {
    ramfs_init_instance(fs);
}
