#pragma once

#include "fs/fs.h"
#include "fs/blockdev.h"
#include "core/stdtypes.h"

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
    uint8_t dirty_blocks[(BLOCKDEV_BLOCK_COUNT + 7) / 8];
    int dirty_any;
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

extern const struct vfs_ops procfs_ops;

int pfs_sync(struct nodefs *fs);
void pfs_mark_dirty_all(struct nodefs *fs);
void pfs_mark_dirty_node(struct nodefs *fs, int node_idx);
void pfs_mark_dirty_node_data(struct nodefs *fs, int node_idx, uint32_t data_off, uint32_t len);
void pfs_mark_dirty_node_size(struct nodefs *fs, int node_idx);
uint32_t pfs_block_count(void);

void ramfs_init_instance(struct nodefs *fs);
void procfs_init_instance(struct nodefs *fs);
