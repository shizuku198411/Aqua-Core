#include "fs_internal.h"
#include "process.h"
#include "kernel.h"
#include "commonlibs.h"
#include "blockdev.h"

extern void syscall_handle_getchar(struct trap_frame *f);

#define VFS_MOUNT_MAX 4
#define PFS_MAGIC 0x50465331u

struct vfs_fd {
    int used;
    int mount_idx;
    int node_index;
    uint32_t offset;
    int flags;
};

static struct vfs_fd fd_table[PROCS_MAX][FS_FD_MAX];

struct fs_node {
    int used;
    int type;
    int parent;
    char name[FS_NAME_MAX];
    uint32_t size;
    uint8_t data[FS_FILE_MAX_SIZE];
};

struct nodefs {
    int mount_idx;
    int persistent;
    struct fs_node nodes[FS_MAX_NODES];
};

struct pfs_image {
    uint32_t magic;
    struct fs_node nodes[FS_MAX_NODES];
};

struct vfs_ops {
    int (*open)(void *ctx, const char *path, int flags, int *node_out, uint32_t *offset_out);
    int (*read)(void *ctx, int node, uint32_t *offset, void *buf, size_t size);
    int (*write)(void *ctx, int node, uint32_t *offset, const void *buf, size_t size);
    int (*mkdir)(void *ctx, const char *path);
    int (*readdir)(void *ctx, const char *path, int index, struct fs_dirent *out);
    int (*unlink)(void *ctx, const char *path);
    int (*rmdir)(void *ctx, const char *path);
};

struct vfs_mount {
    int used;
    char path[FS_PATH_MAX];
    int path_len;
    const struct vfs_ops *ops;
    void *ctx;
};

static struct vfs_mount mounts[VFS_MOUNT_MAX];
static struct nodefs rootfs;
static struct nodefs tmpfs;
static struct pfs_image pfs_work_img;

static int console_read_fallback(void *buf, size_t size) {
    if (!buf) {
        return -1;
    }
    if (size == 0) {
        return 0;
    }

    uint8_t *dst = (uint8_t *) buf;
    for (size_t i = 0; i < size; i++) {
        struct trap_frame f;
        memset(&f, 0, sizeof(f));
        syscall_handle_getchar(&f);
        dst[i] = (uint8_t) f.a0;
    }
    return (int) size;
}

static int console_write_fallback(const void *buf, size_t size) {
    if (!buf) {
        return -1;
    }
    if (size == 0) {
        return 0;
    }

    const uint8_t *src = (const uint8_t *) buf;
    for (size_t i = 0; i < size; i++) {
        putchar((char) src[i]);
    }
    return (int) size;
}

static int copy_name(char *dst, const char *src) {
    int i = 0;
    for (; i < FS_NAME_MAX - 1 && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
    return (src[i] == '\0') ? 0 : -1;
}

static int str_len(const char *s) {
    int n = 0;
    while (s && s[n] != '\0') {
        n++;
    }
    return n;
}

static int next_component(const char *path, int *pos, char *out_name) {
    int i = 0;

    while (path[*pos] == '/') {
        (*pos)++;
    }

    if (path[*pos] == '\0') {
        return 0;
    }

    while (path[*pos] != '\0' && path[*pos] != '/') {
        if (i >= FS_NAME_MAX - 1) {
            return -1;
        }
        out_name[i++] = path[*pos];
        (*pos)++;
    }

    out_name[i] = '\0';
    return 1;
}

static int pfs_block_count(void) {
    return (int) ((sizeof(struct pfs_image) + BLOCKDEV_BLOCK_SIZE - 1) / BLOCKDEV_BLOCK_SIZE);
}

static int pfs_sync(struct nodefs *fs) {
    if (!fs->persistent) {
        return 0;
    }

    struct pfs_image *img = &pfs_work_img;
    img->magic = PFS_MAGIC;
    memcpy(img->nodes, fs->nodes, sizeof(fs->nodes));

    const uint8_t *src = (const uint8_t *) img;
    uint8_t block[BLOCKDEV_BLOCK_SIZE];
    int blocks = pfs_block_count();

    if (blocks > BLOCKDEV_BLOCK_COUNT) {
        return -1;
    }

    for (int i = 0; i < blocks; i++) {
        memset(block, 0, sizeof(block));
        int off = i * BLOCKDEV_BLOCK_SIZE;
        int remain = (int) sizeof(*img) - off;
        int copy_len = remain > BLOCKDEV_BLOCK_SIZE ? BLOCKDEV_BLOCK_SIZE : remain;
        if (copy_len > 0) {
            memcpy(block, src + off, copy_len);
        }
        if (blockdev_write((uint32_t) i, block) < 0) {
            return -1;
        }
    }

    return 0;
}

static void nodefs_format(struct nodefs *fs) {
    memset(fs->nodes, 0, sizeof(fs->nodes));
    fs->nodes[0].used = 1;
    fs->nodes[0].type = FS_TYPE_DIR;
    fs->nodes[0].parent = -1;
    fs->nodes[0].name[0] = '/';
    fs->nodes[0].name[1] = '\0';
}

static void nodefs_init_instance(struct nodefs *fs, int persistent) {
    memset(fs, 0, sizeof(*fs));
    fs->mount_idx = -1;
    fs->persistent = persistent;

    if (!persistent) {
        nodefs_format(fs);
        return;
    }

    struct pfs_image *img = &pfs_work_img;
    uint8_t *dst = (uint8_t *) img;
    uint8_t block[BLOCKDEV_BLOCK_SIZE];
    int blocks = pfs_block_count();

    if (blocks > BLOCKDEV_BLOCK_COUNT) {
        PANIC("pfs image too large for blockdev");
    }

    for (int i = 0; i < blocks; i++) {
        if (blockdev_read((uint32_t) i, block) < 0) {
            PANIC("pfs block read failed");
        }
        int off = i * BLOCKDEV_BLOCK_SIZE;
        int remain = (int) sizeof(*img) - off;
        int copy_len = remain > BLOCKDEV_BLOCK_SIZE ? BLOCKDEV_BLOCK_SIZE : remain;
        if (copy_len > 0) {
            memcpy(dst + off, block, copy_len);
        }
    }

    if (img->magic != PFS_MAGIC) {
        nodefs_format(fs);
        if (pfs_sync(fs) < 0) {
            PANIC("pfs initial sync failed");
        }
        return;
    }

    memcpy(fs->nodes, img->nodes, sizeof(fs->nodes));
    if (!fs->nodes[0].used || fs->nodes[0].type != FS_TYPE_DIR) {
        nodefs_format(fs);
        if (pfs_sync(fs) < 0) {
            PANIC("pfs recovery sync failed");
        }
    }
}

static int nodefs_find_child(struct nodefs *fs, int parent_idx, const char *name) {
    for (int i = 0; i < FS_MAX_NODES; i++) {
        if (!fs->nodes[i].used) {
            continue;
        }
        if (fs->nodes[i].parent != parent_idx) {
            continue;
        }
        if (strcmp(fs->nodes[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int nodefs_alloc_node(struct nodefs *fs) {
    for (int i = 1; i < FS_MAX_NODES; i++) {
        if (!fs->nodes[i].used) {
            fs->nodes[i].used = 1;
            fs->nodes[i].type = 0;
            fs->nodes[i].parent = -1;
            fs->nodes[i].size = 0;
            memset(fs->nodes[i].name, 0, sizeof(fs->nodes[i].name));
            memset(fs->nodes[i].data, 0, sizeof(fs->nodes[i].data));
            return i;
        }
    }
    return -1;
}

static int nodefs_resolve_path(struct nodefs *fs, const char *path) {
    if (!path || path[0] != '/') {
        return -1;
    }

    int cur = 0;
    int pos = 0;
    char name[FS_NAME_MAX];

    while (1) {
        int t = next_component(path, &pos, name);
        if (t < 0) {
            return -1;
        }
        if (t == 0) {
            return cur;
        }

        int child = nodefs_find_child(fs, cur, name);
        if (child < 0) {
            return -1;
        }
        cur = child;
    }
}

static int nodefs_resolve_parent_and_leaf(struct nodefs *fs,
                                          const char *path,
                                          int *parent_out,
                                          char *leaf_out) {
    if (!path || !parent_out || !leaf_out) {
        return -1;
    }
    if (path[0] != '/' || path[1] == '\0') {
        return -1;
    }

    int cur = 0;
    int pos = 0;
    char comp[FS_NAME_MAX];

    int t = next_component(path, &pos, comp);
    if (t <= 0) {
        return -1;
    }

    while (1) {
        int saved_pos = pos;
        char peek[FS_NAME_MAX];
        int has_more = next_component(path, &saved_pos, peek);
        if (has_more < 0) {
            return -1;
        }

        if (has_more == 0) {
            *parent_out = cur;
            return copy_name(leaf_out, comp);
        }

        int child = nodefs_find_child(fs, cur, comp);
        if (child < 0 || fs->nodes[child].type != FS_TYPE_DIR) {
            return -1;
        }

        cur = child;
        pos = saved_pos;
        if (copy_name(comp, peek) < 0) {
            return -1;
        }
    }
}

static int nodefs_is_node_open(struct nodefs *fs, int node_index) {
    if (fs->mount_idx < 0) {
        return 0;
    }

    for (int pid = 0; pid < PROCS_MAX; pid++) {
        for (int fd = 0; fd < FS_FD_MAX; fd++) {
            if (!fd_table[pid][fd].used) {
                continue;
            }
            if (fd_table[pid][fd].mount_idx == fs->mount_idx &&
                fd_table[pid][fd].node_index == node_index) {
                return 1;
            }
        }
    }

    return 0;
}

static int nodefs_is_dir_empty(struct nodefs *fs, int node_index) {
    for (int i = 0; i < FS_MAX_NODES; i++) {
        if (!fs->nodes[i].used) {
            continue;
        }
        if (fs->nodes[i].parent == node_index) {
            return 0;
        }
    }
    return 1;
}

static int nodefs_open(void *ctx,
                       const char *path,
                       int flags,
                       int *node_out,
                       uint32_t *offset_out) {
    struct nodefs *fs = (struct nodefs *) ctx;

    if ((flags & (O_RDONLY | O_WRONLY)) == 0) {
        flags |= O_RDONLY;
    }

    int node = nodefs_resolve_path(fs, path);
    if (node < 0) {
        if ((flags & O_CREAT) == 0) {
            return -1;
        }

        int parent = -1;
        char leaf[FS_NAME_MAX];
        if (nodefs_resolve_parent_and_leaf(fs, path, &parent, leaf) < 0) {
            return -1;
        }
        if (nodefs_find_child(fs, parent, leaf) >= 0) {
            return -1;
        }

        int idx = nodefs_alloc_node(fs);
        if (idx < 0) {
            return -1;
        }

        fs->nodes[idx].type = FS_TYPE_FILE;
        fs->nodes[idx].parent = parent;
        if (copy_name(fs->nodes[idx].name, leaf) < 0) {
            fs->nodes[idx].used = 0;
            return -1;
        }
        if (pfs_sync(fs) < 0) {
            return -1;
        }
        node = idx;
    }

    if (fs->nodes[node].type != FS_TYPE_FILE) {
        return -1;
    }

    if ((flags & O_TRUNC) && (flags & O_WRONLY)) {
        fs->nodes[node].size = 0;
        memset(fs->nodes[node].data, 0, sizeof(fs->nodes[node].data));
        if (pfs_sync(fs) < 0) {
            return -1;
        }
    }

    *node_out = node;
    *offset_out = 0;
    return 0;
}

static int nodefs_read(void *ctx, int node, uint32_t *offset, void *buf, size_t size) {
    struct nodefs *fs = (struct nodefs *) ctx;

    if (node <= 0 || node >= FS_MAX_NODES || !fs->nodes[node].used) {
        return -1;
    }

    struct fs_node *n = &fs->nodes[node];
    if (n->type != FS_TYPE_FILE) {
        return -1;
    }

    if (*offset >= n->size) {
        return 0;
    }

    uint32_t remain = n->size - *offset;
    uint32_t to_read = (uint32_t) size;
    if (to_read > remain) {
        to_read = remain;
    }

    memcpy(buf, &n->data[*offset], to_read);
    *offset += to_read;
    return (int) to_read;
}

static int nodefs_write(void *ctx,
                        int node,
                        uint32_t *offset,
                        const void *buf,
                        size_t size) {
    struct nodefs *fs = (struct nodefs *) ctx;

    if (node <= 0 || node >= FS_MAX_NODES || !fs->nodes[node].used) {
        return -1;
    }

    struct fs_node *n = &fs->nodes[node];
    if (n->type != FS_TYPE_FILE) {
        return -1;
    }

    if (*offset >= FS_FILE_MAX_SIZE) {
        return 0;
    }

    uint32_t writable = FS_FILE_MAX_SIZE - *offset;
    uint32_t to_write = (uint32_t) size;
    if (to_write > writable) {
        to_write = writable;
    }

    memcpy(&n->data[*offset], buf, to_write);
    *offset += to_write;
    if (*offset > n->size) {
        n->size = *offset;
    }

    if (pfs_sync(fs) < 0) {
        return -1;
    }

    return (int) to_write;
}

static int nodefs_mkdir(void *ctx, const char *path) {
    struct nodefs *fs = (struct nodefs *) ctx;

    int parent = -1;
    char leaf[FS_NAME_MAX];

    if (nodefs_resolve_parent_and_leaf(fs, path, &parent, leaf) < 0) {
        return -1;
    }

    if (nodefs_find_child(fs, parent, leaf) >= 0) {
        return -1;
    }

    int idx = nodefs_alloc_node(fs);
    if (idx < 0) {
        return -1;
    }

    fs->nodes[idx].type = FS_TYPE_DIR;
    fs->nodes[idx].parent = parent;
    if (copy_name(fs->nodes[idx].name, leaf) < 0) {
        fs->nodes[idx].used = 0;
        return -1;
    }

    if (pfs_sync(fs) < 0) {
        return -1;
    }

    return 0;
}

static int nodefs_readdir(void *ctx, const char *path, int index, struct fs_dirent *out) {
    struct nodefs *fs = (struct nodefs *) ctx;

    if (index < 0) {
        return -1;
    }

    int dir = nodefs_resolve_path(fs, path);
    if (dir < 0 || fs->nodes[dir].type != FS_TYPE_DIR) {
        return -1;
    }

    int seen = 0;
    for (int i = 0; i < FS_MAX_NODES; i++) {
        if (!fs->nodes[i].used || fs->nodes[i].parent != dir) {
            continue;
        }

        if (seen == index) {
            memset(out, 0, sizeof(*out));
            copy_name(out->name, fs->nodes[i].name);
            out->type = fs->nodes[i].type;
            out->size = fs->nodes[i].size;
            return 0;
        }

        seen++;
    }

    return -1;
}

static int nodefs_unlink(void *ctx, const char *path) {
    struct nodefs *fs = (struct nodefs *) ctx;

    int node = nodefs_resolve_path(fs, path);
    if (node <= 0) {
        return -1;
    }
    if (fs->nodes[node].type != FS_TYPE_FILE) {
        return -1;
    }
    if (nodefs_is_node_open(fs, node)) {
        return -1;
    }

    memset(&fs->nodes[node], 0, sizeof(fs->nodes[node]));
    if (pfs_sync(fs) < 0) {
        return -1;
    }
    return 0;
}

static int nodefs_rmdir(void *ctx, const char *path) {
    struct nodefs *fs = (struct nodefs *) ctx;

    int node = nodefs_resolve_path(fs, path);
    if (node <= 0) {
        return -1;
    }
    if (fs->nodes[node].type != FS_TYPE_DIR) {
        return -1;
    }
    if (!nodefs_is_dir_empty(fs, node)) {
        return -1;
    }

    memset(&fs->nodes[node], 0, sizeof(fs->nodes[node]));
    if (pfs_sync(fs) < 0) {
        return -1;
    }
    return 0;
}

static const struct vfs_ops nodefs_ops = {
    .open = nodefs_open,
    .read = nodefs_read,
    .write = nodefs_write,
    .mkdir = nodefs_mkdir,
    .readdir = nodefs_readdir,
    .unlink = nodefs_unlink,
    .rmdir = nodefs_rmdir,
};

static int vfs_mount(const char *path, const struct vfs_ops *ops, void *ctx) {
    if (!path || !ops || !ctx) {
        return -1;
    }

    int path_len = str_len(path);
    if (path_len <= 0 || path_len >= FS_PATH_MAX || path[0] != '/') {
        return -1;
    }

    for (int i = 0; i < VFS_MOUNT_MAX; i++) {
        if (!mounts[i].used) {
            mounts[i].used = 1;
            mounts[i].path_len = path_len;
            mounts[i].ops = ops;
            mounts[i].ctx = ctx;
            memset(mounts[i].path, 0, sizeof(mounts[i].path));
            for (int j = 0; j < path_len; j++) {
                mounts[i].path[j] = path[j];
            }

            ((struct nodefs *) ctx)->mount_idx = i;
            return 0;
        }
    }

    return -1;
}

static int vfs_match_mount(struct vfs_mount *m, const char *path) {
    if (m->path_len == 1 && m->path[0] == '/') {
        return 1;
    }

    for (int i = 0; i < m->path_len; i++) {
        if (path[i] != m->path[i]) {
            return 0;
        }
    }

    char next = path[m->path_len];
    return next == '\0' || next == '/';
}

static int vfs_resolve_mount(const char *path,
                             struct vfs_mount **mount_out,
                             const char **subpath_out) {
    if (!path || path[0] != '/') {
        return -1;
    }

    struct vfs_mount *best = NULL;
    int best_len = -1;

    for (int i = 0; i < VFS_MOUNT_MAX; i++) {
        if (!mounts[i].used) {
            continue;
        }
        if (!vfs_match_mount(&mounts[i], path)) {
            continue;
        }
        if (mounts[i].path_len > best_len) {
            best = &mounts[i];
            best_len = mounts[i].path_len;
        }
    }

    if (!best) {
        return -1;
    }

    const char *subpath;
    if (best_len == 1) {
        subpath = path;
    } else {
        subpath = path + best_len;
        if (*subpath == '\0') {
            subpath = "/";
        }
    }

    if (subpath[0] != '/') {
        return -1;
    }

    *mount_out = best;
    *subpath_out = subpath;
    return 0;
}

static int vfs_alloc_fd(int pid, int mount_idx, int node_index, uint32_t offset, int flags) {
    if (pid < 0 || pid >= PROCS_MAX) {
        return -1;
    }

    for (int fd = 0; fd < FS_FD_MAX; fd++) {
        if (!fd_table[pid][fd].used) {
            fd_table[pid][fd].used = 1;
            fd_table[pid][fd].mount_idx = mount_idx;
            fd_table[pid][fd].node_index = node_index;
            fd_table[pid][fd].offset = offset;
            fd_table[pid][fd].flags = flags;
            return fd;
        }
    }

    return -1;
}

void fs_init(void) {
    printf("\n");
    printf("     [fs] reset fd/mount tables...");
    memset(fd_table, 0, sizeof(fd_table));
    memset(mounts, 0, sizeof(mounts));
    printf("OK\n");

    printf("     [fs] init block device (virtio-blk)...");
    blockdev_init();
    printf("OK\n");

    // Root is persistent backend (PFS on blockdev abstraction).
    printf("     [fs] init rootfs (persistent pfs)...");
    nodefs_init_instance(&rootfs, 1);
    printf("OK\n");

    // /tmp is volatile RAMFS backend.
    printf("     [fs] init tmpfs (volatile ramfs)...");
    nodefs_init_instance(&tmpfs, 0);
    printf("OK\n");

    printf("     [fs] mount: / -> rootfs ...");
    if (vfs_mount("/", &nodefs_ops, &rootfs) < 0) {
        PANIC("failed to mount root fs");
    }
    printf("OK\n");

    // Ensure mountpoint exists in root namespace for `ls /`.
    if (nodefs_resolve_path(&rootfs, "/tmp") < 0) {
        printf("     [fs] create mountpoint: /tmp ...");
        (void) nodefs_mkdir(&rootfs, "/tmp");
        printf("OK\n");
    }

    printf("     [fs] mount: /tmp -> tmpfs ...");
    if (vfs_mount("/tmp", &nodefs_ops, &tmpfs) < 0) {
        PANIC("failed to mount tmp fs");
    }
    printf("OK\n");
}

int fs_open(int pid, const char *path, int flags) {
    struct vfs_mount *m = NULL;
    const char *subpath = NULL;

    if (pid < 0 || pid >= PROCS_MAX || !path) {
        return -1;
    }
    if (vfs_resolve_mount(path, &m, &subpath) < 0) {
        return -1;
    }

    int node = -1;
    uint32_t offset = 0;
    if (m->ops->open(m->ctx, subpath, flags, &node, &offset) < 0) {
        return -1;
    }

    return vfs_alloc_fd(pid, (int) (m - mounts), node, offset, flags);
}

int fs_close(int pid, int fd) {
    if (pid < 0 || pid >= PROCS_MAX) {
        return -1;
    }
    if (fd < 0 || fd >= FS_FD_MAX || !fd_table[pid][fd].used) {
        return -1;
    }

    fd_table[pid][fd].used = 0;
    fd_table[pid][fd].mount_idx = -1;
    fd_table[pid][fd].node_index = -1;
    fd_table[pid][fd].offset = 0;
    fd_table[pid][fd].flags = 0;
    return 0;
}

int fs_read(int pid, int fd, void *buf, size_t size) {
    if (pid < 0 || pid >= PROCS_MAX || !buf) {
        return -1;
    }
    if (fd < 0 || fd >= FS_FD_MAX) {
        return -1;
    }
    if (!fd_table[pid][fd].used) {
        return (fd == 0) ? console_read_fallback(buf, size) : -1;
    }

    struct vfs_fd *f = &fd_table[pid][fd];
    if ((f->flags & O_RDONLY) == 0) {
        return -1;
    }

    if (f->mount_idx < 0 || f->mount_idx >= VFS_MOUNT_MAX || !mounts[f->mount_idx].used) {
        return -1;
    }

    return mounts[f->mount_idx].ops->read(mounts[f->mount_idx].ctx,
                                          f->node_index,
                                          &f->offset,
                                          buf,
                                          size);
}

int fs_write(int pid, int fd, const void *buf, size_t size) {
    if (pid < 0 || pid >= PROCS_MAX || !buf) {
        return -1;
    }
    if (fd < 0 || fd >= FS_FD_MAX) {
        return -1;
    }
    if (!fd_table[pid][fd].used) {
        return (fd == 1) ? console_write_fallback(buf, size) : -1;
    }

    struct vfs_fd *f = &fd_table[pid][fd];
    if ((f->flags & O_WRONLY) == 0) {
        return -1;
    }

    if (f->mount_idx < 0 || f->mount_idx >= VFS_MOUNT_MAX || !mounts[f->mount_idx].used) {
        return -1;
    }

    return mounts[f->mount_idx].ops->write(mounts[f->mount_idx].ctx,
                                           f->node_index,
                                           &f->offset,
                                           buf,
                                           size);
}

int fs_mkdir(const char *path) {
    struct vfs_mount *m = NULL;
    const char *subpath = NULL;

    if (!path) {
        return -1;
    }
    if (vfs_resolve_mount(path, &m, &subpath) < 0) {
        return -1;
    }

    return m->ops->mkdir(m->ctx, subpath);
}

int fs_readdir(const char *path, int index, struct fs_dirent *out) {
    struct vfs_mount *m = NULL;
    const char *subpath = NULL;

    if (!path || !out || index < 0) {
        return -1;
    }
    if (vfs_resolve_mount(path, &m, &subpath) < 0) {
        return -1;
    }

    return m->ops->readdir(m->ctx, subpath, index, out);
}

int fs_unlink(const char *path) {
    struct vfs_mount *m = NULL;
    const char *subpath = NULL;

    if (!path) {
        return -1;
    }
    if (vfs_resolve_mount(path, &m, &subpath) < 0) {
        return -1;
    }

    return m->ops->unlink(m->ctx, subpath);
}

int fs_rmdir(const char *path) {
    struct vfs_mount *m = NULL;
    const char *subpath = NULL;

    if (!path) {
        return -1;
    }
    if (vfs_resolve_mount(path, &m, &subpath) < 0) {
        return -1;
    }

    return m->ops->rmdir(m->ctx, subpath);
}

int fs_dup2(int pid, int old_fd, int new_fd) {
    if (pid < 0 || pid >= PROCS_MAX) return -1;
    if ((old_fd < 0 || old_fd >= FS_FD_MAX) || !fd_table[pid][old_fd].used) return -1;
    if (new_fd < 0 || new_fd >= FS_FD_MAX) return -1;

    if (old_fd == new_fd) return new_fd;

    // close if new fd is already used
    if (fd_table[pid][new_fd].used) {
        if (fs_close(pid, new_fd) < 0) {
            return -1;
        }
    }
    // copy old_fd to new_fd
    fd_table[pid][new_fd] = fd_table[pid][old_fd];
    fd_table[pid][new_fd].used = 1;
    return new_fd;
}

void fs_on_process_recycle(int pid) {
    if (pid < 0 || pid >= PROCS_MAX) {
        return;
    }

    for (int fd = 0; fd < FS_FD_MAX; fd++) {
        fd_table[pid][fd].used = 0;
        fd_table[pid][fd].mount_idx = -1;
        fd_table[pid][fd].node_index = -1;
        fd_table[pid][fd].offset = 0;
        fd_table[pid][fd].flags = 0;
    }
}

uint32_t fs_get_pfs_image_blocks(void) {
    return (uint32_t) pfs_block_count();
}
