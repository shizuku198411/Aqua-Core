#include "blockdev.h"
#include "kernel.h"
#include "stdtypes.h"
#include "commonlibs.h"

#define VIRTIO_MMIO_BASE       0x10001000u
#define VIRTIO_MMIO_STRIDE     0x1000u
#define VIRTIO_MMIO_MAX_DEVS   8

#define VIRTIO_MMIO_MAGIC_VALUE    0x000
#define VIRTIO_MMIO_VERSION        0x004
#define VIRTIO_MMIO_DEVICE_ID      0x008
#define VIRTIO_MMIO_VENDOR_ID      0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES     0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014
#define VIRTIO_MMIO_DRIVER_FEATURES     0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024
#define VIRTIO_MMIO_QUEUE_SEL      0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX  0x034
#define VIRTIO_MMIO_QUEUE_NUM      0x038
#define VIRTIO_MMIO_QUEUE_READY    0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY   0x050
#define VIRTIO_MMIO_STATUS         0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW  0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH 0x084
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW  0x090
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH 0x094
#define VIRTIO_MMIO_QUEUE_USED_LOW  0x0a0
#define VIRTIO_MMIO_QUEUE_USED_HIGH 0x0a4
#define VIRTIO_MMIO_CONFIG         0x100

#define VIRTIO_MAGIC 0x74726976u
#define VIRTIO_VENDOR 0x554d4551u
#define VIRTIO_DEV_BLOCK 2u

#define VIRTIO_STATUS_ACKNOWLEDGE 1u
#define VIRTIO_STATUS_DRIVER      2u
#define VIRTIO_STATUS_FEATURES_OK 8u
#define VIRTIO_STATUS_DRIVER_OK   4u
#define VIRTIO_STATUS_FAILED      128u

#define VIRTQ_DESC_F_NEXT  1u
#define VIRTQ_DESC_F_WRITE 2u

#define VIRTIO_BLK_T_IN  0u
#define VIRTIO_BLK_T_OUT 1u

#define VIRTIO_BLK_F_RO            5
#define VIRTIO_BLK_F_SCSI          7
#define VIRTIO_BLK_F_CONFIG_WCE    11
#define VIRTIO_BLK_F_MQ            12
#define VIRTIO_F_VERSION_1         32
#define VIRTIO_F_ANY_LAYOUT        27
#define VIRTIO_RING_F_INDIRECT_DESC 28
#define VIRTIO_RING_F_EVENT_IDX    29

#define VQ_NUM 8
#define VQ_ALIGN 4096
#define VQ_BYTES (2 * VQ_ALIGN)

struct virtq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

struct virtq_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VQ_NUM];
    uint16_t used_event;
} __attribute__((packed));

struct virtq_used_elem {
    uint32_t id;
    uint32_t len;
} __attribute__((packed));

struct virtq_used {
    uint16_t flags;
    uint16_t idx;
    struct virtq_used_elem ring[VQ_NUM];
    uint16_t avail_event;
} __attribute__((packed));

struct virtio_blk_req_hdr {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} __attribute__((packed));

static volatile uint32_t *virtio_mmio;
static uint32_t capacity_blocks;

static uint8_t vq_mem[VQ_BYTES] __attribute__((aligned(VQ_ALIGN)));
static struct virtq_desc *vq_desc;
static struct virtq_avail *vq_avail;
static volatile struct virtq_used *vq_used;
static uint16_t vq_last_used_idx;
static struct virtio_blk_req_hdr req_hdr;
static uint8_t req_status;
static int blk_log_once;

static inline void fence_rw_rw(void) {
    __asm__ __volatile__("fence rw, rw" ::: "memory");
}

static inline uint32_t mmio_read(uint32_t off) {
    return *(volatile uint32_t *) ((uint32_t) virtio_mmio + off);
}

static inline void mmio_write(uint32_t off, uint32_t val) {
    *(volatile uint32_t *) ((uint32_t) virtio_mmio + off) = val;
}

static int virtio_find_blk(void) {
    for (uint32_t i = 0; i < VIRTIO_MMIO_MAX_DEVS; i++) {
        volatile uint32_t *base = (volatile uint32_t *) (VIRTIO_MMIO_BASE + i * VIRTIO_MMIO_STRIDE);
        uint32_t magic = *(volatile uint32_t *) ((uint32_t) base + VIRTIO_MMIO_MAGIC_VALUE);
        uint32_t version = *(volatile uint32_t *) ((uint32_t) base + VIRTIO_MMIO_VERSION);
        uint32_t dev_id = *(volatile uint32_t *) ((uint32_t) base + VIRTIO_MMIO_DEVICE_ID);
        uint32_t vendor = *(volatile uint32_t *) ((uint32_t) base + VIRTIO_MMIO_VENDOR_ID);

        if (magic != VIRTIO_MAGIC) {
            continue;
        }
        if (version < 2) {
            continue;
        }
        if (dev_id != VIRTIO_DEV_BLOCK) {
            continue;
        }
        if (vendor != VIRTIO_VENDOR) {
            continue;
        }

        virtio_mmio = base;
        return 0;
    }

    return -1;
}

static void virtio_setup_vq_layout(void) {
    uint32_t desc_bytes = (uint32_t) sizeof(struct virtq_desc) * VQ_NUM;

    vq_desc = (struct virtq_desc *) &vq_mem[0];
    vq_avail = (struct virtq_avail *) &vq_mem[desc_bytes];
    vq_used = (volatile struct virtq_used *) &vq_mem[VQ_ALIGN];

    memset(vq_mem, 0, sizeof(vq_mem));
    vq_last_used_idx = 0;
}

static int virtio_setup_queue(void) {
    mmio_write(VIRTIO_MMIO_QUEUE_SEL, 0);
    uint32_t qmax = mmio_read(VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (qmax == 0 || qmax < VQ_NUM) {
        return -1;
    }

    mmio_write(VIRTIO_MMIO_QUEUE_NUM, VQ_NUM);
    mmio_write(VIRTIO_MMIO_QUEUE_READY, 0);

    uint32_t desc = (uint32_t) vq_desc;
    uint32_t avail = (uint32_t) vq_avail;
    uint32_t used = (uint32_t) vq_used;
    mmio_write(VIRTIO_MMIO_QUEUE_DESC_LOW, desc);
    mmio_write(VIRTIO_MMIO_QUEUE_DESC_HIGH, 0);
    mmio_write(VIRTIO_MMIO_QUEUE_AVAIL_LOW, avail);
    mmio_write(VIRTIO_MMIO_QUEUE_AVAIL_HIGH, 0);
    mmio_write(VIRTIO_MMIO_QUEUE_USED_LOW, used);
    mmio_write(VIRTIO_MMIO_QUEUE_USED_HIGH, 0);
    mmio_write(VIRTIO_MMIO_QUEUE_READY, 1);

    return 0;
}

static int virtio_do_io(uint32_t type, uint32_t block_index, void *buf) {
    req_hdr.type = type;
    req_hdr.reserved = 0;
    req_hdr.sector = block_index;
    req_status = 0xff;

    vq_desc[0].addr = (uint64_t) (uint32_t) &req_hdr;
    vq_desc[0].len = sizeof(req_hdr);
    vq_desc[0].flags = VIRTQ_DESC_F_NEXT;
    vq_desc[0].next = 1;

    vq_desc[1].addr = (uint64_t) (uint32_t) buf;
    vq_desc[1].len = BLOCKDEV_BLOCK_SIZE;
    vq_desc[1].flags = VIRTQ_DESC_F_NEXT;
    if (type == VIRTIO_BLK_T_IN) {
        vq_desc[1].flags |= VIRTQ_DESC_F_WRITE;
    }
    vq_desc[1].next = 2;

    vq_desc[2].addr = (uint64_t) (uint32_t) &req_status;
    vq_desc[2].len = 1;
    vq_desc[2].flags = VIRTQ_DESC_F_WRITE;
    vq_desc[2].next = 0;

    uint16_t avail_idx = vq_avail->idx;
    vq_avail->ring[avail_idx % VQ_NUM] = 0;
    fence_rw_rw();
    vq_avail->idx = (uint16_t) (avail_idx + 1);

    fence_rw_rw();
    mmio_write(VIRTIO_MMIO_QUEUE_NOTIFY, 0);

    uint32_t spin = 0;
    while (vq_used->idx == vq_last_used_idx) {
        __asm__ __volatile__("nop");
        if (++spin > 3000000u) {
            if (!blk_log_once) {
                printf("[blk] timeout type=%u blk=%u status=%x used=%u last=%u\n",
                       type, block_index, req_status, vq_used->idx, vq_last_used_idx);
                blk_log_once = 1;
            }
            return -1;
        }
    }

    vq_last_used_idx = vq_used->idx;
    fence_rw_rw();

    if (req_status != 0) {
        if (!blk_log_once) {
            printf("[blk] io err type=%u blk=%u status=%x\n", type, block_index, req_status);
            blk_log_once = 1;
        }
        return -1;
    }

    return 0;
}

void blockdev_init(void) {
    if (virtio_find_blk() < 0) {
        PANIC("virtio-blk(modern) not found");
    }

    virtio_setup_vq_layout();

    mmio_write(VIRTIO_MMIO_STATUS, 0);
    mmio_write(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);
    mmio_write(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

    mmio_write(VIRTIO_MMIO_DEVICE_FEATURES_SEL, 0);
    uint32_t features0 = mmio_read(VIRTIO_MMIO_DEVICE_FEATURES);
    mmio_write(VIRTIO_MMIO_DEVICE_FEATURES_SEL, 1);
    uint32_t features1 = mmio_read(VIRTIO_MMIO_DEVICE_FEATURES);
    (void) features0;

    // Modern virtio-mmio requires VIRTIO_F_VERSION_1 negotiation.
    if ((features1 & (1u << (VIRTIO_F_VERSION_1 - 32))) == 0) {
        mmio_write(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_FAILED);
        PANIC("virtio-blk missing VERSION_1");
    }

    // Keep negotiated features minimal: only VERSION_1.
    mmio_write(VIRTIO_MMIO_DRIVER_FEATURES_SEL, 0);
    mmio_write(VIRTIO_MMIO_DRIVER_FEATURES, 0);
    mmio_write(VIRTIO_MMIO_DRIVER_FEATURES_SEL, 1);
    mmio_write(VIRTIO_MMIO_DRIVER_FEATURES, (1u << (VIRTIO_F_VERSION_1 - 32)));

    uint32_t status = mmio_read(VIRTIO_MMIO_STATUS);
    status |= VIRTIO_STATUS_FEATURES_OK;
    mmio_write(VIRTIO_MMIO_STATUS, status);
    if ((mmio_read(VIRTIO_MMIO_STATUS) & VIRTIO_STATUS_FEATURES_OK) == 0) {
        mmio_write(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_FAILED);
        PANIC("virtio-blk FEATURES_OK rejected");
    }

    if (virtio_setup_queue() < 0) {
        mmio_write(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_FAILED);
        PANIC("virtio-blk queue setup failed");
    }

    uint32_t cap_lo = mmio_read(VIRTIO_MMIO_CONFIG + 0x00);
    uint32_t cap_hi = mmio_read(VIRTIO_MMIO_CONFIG + 0x04);
    uint64_t cap = ((uint64_t) cap_hi << 32) | cap_lo;
    if (cap == 0) {
        mmio_write(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_FAILED);
        PANIC("virtio-blk capacity is zero");
    }
    if (cap > 0xffffffffu) {
        capacity_blocks = 0xffffffffu;
    } else {
        capacity_blocks = (uint32_t) cap;
    }

    mmio_write(VIRTIO_MMIO_STATUS,
               VIRTIO_STATUS_ACKNOWLEDGE |
               VIRTIO_STATUS_DRIVER |
               VIRTIO_STATUS_DRIVER_OK);

    uint8_t probe[BLOCKDEV_BLOCK_SIZE];
    if (virtio_do_io(VIRTIO_BLK_T_IN, 0, probe) < 0) {
        mmio_write(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_FAILED);
        PANIC("virtio-blk probe io failed");
    }
}

int blockdev_read(uint32_t block_index, void *out_block) {
    if (!out_block) {
        return -1;
    }
    if (block_index >= BLOCKDEV_BLOCK_COUNT || block_index >= capacity_blocks) {
        return -1;
    }

    return virtio_do_io(VIRTIO_BLK_T_IN, block_index, out_block);
}

int blockdev_write(uint32_t block_index, const void *in_block) {
    if (!in_block) {
        return -1;
    }
    if (block_index >= BLOCKDEV_BLOCK_COUNT || block_index >= capacity_blocks) {
        return -1;
    }

    return virtio_do_io(VIRTIO_BLK_T_OUT, block_index, (void *) in_block);
}
