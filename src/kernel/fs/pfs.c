#include "fs/internal.h"
#include "fs_private.h"
#include "core/commonlibs.h"
#include "kernel/kernel.h"
#include "fs/blockdev.h"

#define PFS_MAGIC 0x50465331u

struct pfs_image {
    uint32_t magic;
    struct fs_node nodes[FS_MAX_NODES];
};

static struct pfs_image pfs_work_img;

uint32_t pfs_block_count(void) {
    return (uint32_t) ((sizeof(struct pfs_image) + BLOCKDEV_BLOCK_SIZE - 1) / BLOCKDEV_BLOCK_SIZE);
}

static int pfs_dirty_test(struct nodefs *fs, int block_idx) {
    if (!fs || block_idx < 0 || block_idx >= BLOCKDEV_BLOCK_COUNT) {
        return 0;
    }
    return (fs->dirty_blocks[block_idx / 8] >> (block_idx % 8)) & 1;
}

static void pfs_dirty_set(struct nodefs *fs, int block_idx) {
    if (!fs || block_idx < 0 || block_idx >= BLOCKDEV_BLOCK_COUNT) {
        return;
    }
    fs->dirty_blocks[block_idx / 8] |= (uint8_t) (1u << (block_idx % 8));
    fs->dirty_any = 1;
}

static void pfs_mark_dirty_range(struct nodefs *fs, uint32_t off, uint32_t len) {
    if (!fs || len == 0) {
        return;
    }

    uint32_t end = off + len;
    uint32_t first = off / BLOCKDEV_BLOCK_SIZE;
    uint32_t last = (end - 1) / BLOCKDEV_BLOCK_SIZE;
    for (uint32_t b = first; b <= last; b++) {
        pfs_dirty_set(fs, (int) b);
    }
}

void pfs_mark_dirty_all(struct nodefs *fs) {
    int blocks = (int) pfs_block_count();
    for (int i = 0; i < blocks; i++) {
        pfs_dirty_set(fs, i);
    }
}

void pfs_mark_dirty_node(struct nodefs *fs, int node_idx) {
    if (!fs || node_idx < 0 || node_idx >= FS_MAX_NODES) {
        return;
    }

    uint32_t node_off = (uint32_t) offsetof(struct pfs_image, nodes) +
                        (uint32_t) node_idx * (uint32_t) sizeof(struct fs_node);
    pfs_mark_dirty_range(fs, node_off, (uint32_t) sizeof(struct fs_node));
}

void pfs_mark_dirty_node_data(struct nodefs *fs, int node_idx, uint32_t data_off, uint32_t len) {
    if (!fs || node_idx < 0 || node_idx >= FS_MAX_NODES || len == 0 || data_off >= FS_FILE_MAX_SIZE) {
        return;
    }

    uint32_t clipped = len;
    if (clipped > FS_FILE_MAX_SIZE - data_off) {
        clipped = FS_FILE_MAX_SIZE - data_off;
    }
    uint32_t off = (uint32_t) offsetof(struct pfs_image, nodes) +
                   (uint32_t) node_idx * (uint32_t) sizeof(struct fs_node) +
                   (uint32_t) offsetof(struct fs_node, data) +
                   data_off;
    pfs_mark_dirty_range(fs, off, clipped);
}

void pfs_mark_dirty_node_size(struct nodefs *fs, int node_idx) {
    if (!fs || node_idx < 0 || node_idx >= FS_MAX_NODES) {
        return;
    }
    uint32_t off = (uint32_t) offsetof(struct pfs_image, nodes) +
                   (uint32_t) node_idx * (uint32_t) sizeof(struct fs_node) +
                   (uint32_t) offsetof(struct fs_node, size);
    pfs_mark_dirty_range(fs, off, (uint32_t) sizeof(uint32_t));
}

int pfs_sync(struct nodefs *fs) {
    if (!fs || !fs->persistent) {
        return 0;
    }
    if (!fs->dirty_any) {
        return 0;
    }

    struct pfs_image *img = &pfs_work_img;
    img->magic = PFS_MAGIC;
    memcpy(img->nodes, fs->nodes, sizeof(fs->nodes));

    const uint8_t *src = (const uint8_t *) img;
    uint8_t block[BLOCKDEV_BLOCK_SIZE];
    int blocks = (int) pfs_block_count();

    if (blocks > BLOCKDEV_BLOCK_COUNT) {
        return -1;
    }

    for (int i = 0; i < blocks; i++) {
        if (!pfs_dirty_test(fs, i)) {
            continue;
        }
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
        fs->dirty_blocks[i / 8] &= (uint8_t) ~(1u << (i % 8));
    }

    fs->dirty_any = 0;
    return 0;
}

uint32_t fs_get_pfs_image_blocks(void) {
    return pfs_block_count();
}

uint32_t fs_get_pfs_sync_mode(void) {
    return KERNEL_PFS_SYNC_MODE_DIRTY;
}
