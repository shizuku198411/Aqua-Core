#include "net.h"
#include "virtio.h"
#include "stdtypes.h"
#include "commonlibs.h"

#define NET_VQ_NUM    8u
#define NET_VQ_ALIGN  4096u
#define NET_VQ_BYTES  (2u * NET_VQ_ALIGN)

struct virtq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

struct virtq_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[NET_VQ_NUM];
    uint16_t used_event;
} __attribute__((packed));

struct virtq_used_elem {
    uint32_t id;
    uint32_t len;
} __attribute__((packed));

struct virtq_used {
    uint16_t flags;
    uint16_t idx;
    struct virtq_used_elem ring[NET_VQ_NUM];
    uint16_t avail_event;
} __attribute__((packed));

struct virtio_queue {
    uint8_t mem[NET_VQ_BYTES] __attribute__((aligned(NET_VQ_ALIGN)));
    struct virtq_desc *desc;
    struct virtq_avail *avail;
    volatile struct virtq_used *used;
};

static volatile uint32_t *virtio_net_mmio;
static struct virtio_queue net_rxq;
static struct virtio_queue net_txq;
static uint8_t net_mac[6];

static inline uint32_t mmio_read(uint32_t off) {
    return *(volatile uint32_t *) ((uint32_t) virtio_net_mmio + off);
}

static inline void mmio_write(uint32_t off, uint32_t val) {
    *(volatile uint32_t *) ((uint32_t) virtio_net_mmio + off) = val;
}

static int virtio_find_net(void) {
    for (uint32_t i = 0; i < VIRTIO_MMIO_MAX_DEVS; i++) {
        volatile uint32_t *base = (volatile uint32_t *) (VIRTIO_MMIO_BASE + i * VIRTIO_MMIO_STRIDE);
        uint32_t magic = *(volatile uint32_t *) ((uint32_t) base + VIRTIO_MMIO_MAGIC_VALUE);
        uint32_t version = *(volatile uint32_t *) ((uint32_t) base + VIRTIO_MMIO_VERSION);
        uint32_t dev_id = *(volatile uint32_t *) ((uint32_t) base + VIRTIO_MMIO_DEVICE_ID);
        uint32_t vendor = *(volatile uint32_t *) ((uint32_t) base + VIRTIO_MMIO_VENDOR_ID);

        if (magic != VIRTIO_MAGIC) {
            continue;
        }
        if (version < 2u) {
            continue;
        }
        if (dev_id != VIRTIO_DEV_NET) {
            continue;
        }
        if (vendor != VIRTIO_VENDOR) {
            continue;
        }

        virtio_net_mmio = base;
        return 0;
    }

    return -1;
}

static void queue_setup_layout(struct virtio_queue *q) {
    uint32_t desc_bytes = (uint32_t) sizeof(struct virtq_desc) * NET_VQ_NUM;

    memset(q->mem, 0, sizeof(q->mem));
    q->desc = (struct virtq_desc *) &q->mem[0];
    q->avail = (struct virtq_avail *) &q->mem[desc_bytes];
    q->used = (volatile struct virtq_used *) &q->mem[NET_VQ_ALIGN];
}

static int queue_bind(uint32_t qsel, struct virtio_queue *q) {
    mmio_write(VIRTIO_MMIO_QUEUE_SEL, qsel);

    uint32_t qmax = mmio_read(VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (qmax == 0u || qmax < NET_VQ_NUM) {
        return -1;
    }

    mmio_write(VIRTIO_MMIO_QUEUE_NUM, NET_VQ_NUM);
    mmio_write(VIRTIO_MMIO_QUEUE_READY, 0);

    uint32_t desc = (uint32_t) q->desc;
    uint32_t avail = (uint32_t) q->avail;
    uint32_t used = (uint32_t) q->used;

    mmio_write(VIRTIO_MMIO_QUEUE_DESC_LOW, desc);
    mmio_write(VIRTIO_MMIO_QUEUE_DESC_HIGH, 0);
    mmio_write(VIRTIO_MMIO_QUEUE_AVAIL_LOW, avail);
    mmio_write(VIRTIO_MMIO_QUEUE_AVAIL_HIGH, 0);
    mmio_write(VIRTIO_MMIO_QUEUE_USED_LOW, used);
    mmio_write(VIRTIO_MMIO_QUEUE_USED_HIGH, 0);
    mmio_write(VIRTIO_MMIO_QUEUE_READY, 1);
    return 0;
}

static int configure_device(void) {
    mmio_write(VIRTIO_MMIO_STATUS, 0);
    mmio_write(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);
    mmio_write(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

    mmio_write(VIRTIO_MMIO_DEVICE_FEATURES_SEL, 0);
    uint32_t features0 = mmio_read(VIRTIO_MMIO_DEVICE_FEATURES);
    mmio_write(VIRTIO_MMIO_DEVICE_FEATURES_SEL, 1);
    uint32_t features1 = mmio_read(VIRTIO_MMIO_DEVICE_FEATURES);
    (void) features0;

    if ((features1 & (1u << (VIRTIO_F_VERSION_1 - 32u))) == 0u) {
        return -1;
    }

    // Minimal handshake: only VERSION_1.
    mmio_write(VIRTIO_MMIO_DRIVER_FEATURES_SEL, 0);
    mmio_write(VIRTIO_MMIO_DRIVER_FEATURES, 0);
    mmio_write(VIRTIO_MMIO_DRIVER_FEATURES_SEL, 1);
    mmio_write(VIRTIO_MMIO_DRIVER_FEATURES, (1u << (VIRTIO_F_VERSION_1 - 32u)));

    uint32_t status = mmio_read(VIRTIO_MMIO_STATUS);
    status |= VIRTIO_STATUS_FEATURES_OK;
    mmio_write(VIRTIO_MMIO_STATUS, status);
    if ((mmio_read(VIRTIO_MMIO_STATUS) & VIRTIO_STATUS_FEATURES_OK) == 0u) {
        return -1;
    }

    if (queue_bind(0, &net_rxq) < 0) {
        return -1;
    }
    if (queue_bind(1, &net_txq) < 0) {
        return -1;
    }

    status = mmio_read(VIRTIO_MMIO_STATUS);
    status |= VIRTIO_STATUS_DRIVER_OK;
    mmio_write(VIRTIO_MMIO_STATUS, status);
    return 0;
}

static void read_mac_addr(void) {
    uint32_t lo = mmio_read(VIRTIO_MMIO_CONFIG + 0x00u);
    uint32_t hi = mmio_read(VIRTIO_MMIO_CONFIG + 0x04u);

    net_mac[0] = (uint8_t) (lo & 0xffu);
    net_mac[1] = (uint8_t) ((lo >> 8) & 0xffu);
    net_mac[2] = (uint8_t) ((lo >> 16) & 0xffu);
    net_mac[3] = (uint8_t) ((lo >> 24) & 0xffu);
    net_mac[4] = (uint8_t) (hi & 0xffu);
    net_mac[5] = (uint8_t) ((hi >> 8) & 0xffu);
}

int net_init(void) {
    if (virtio_find_net() < 0) {
        printf("     [net] virtio-net(modern) not found\n");
        return -1;
    }

    queue_setup_layout(&net_rxq);
    queue_setup_layout(&net_txq);

    if (configure_device() < 0) {
        mmio_write(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_FAILED);
        printf("     [net] virtio-net configure failed\n");
        return -1;
    }

    read_mac_addr();
    printf("     [net] virtio-net init OK:\n           mac=%x:%x:%x:%x:%x:%x\n",
           (unsigned) net_mac[0],
           (unsigned) net_mac[1],
           (unsigned) net_mac[2],
           (unsigned) net_mac[3],
           (unsigned) net_mac[4],
           (unsigned) net_mac[5]);
    return 0;
}
