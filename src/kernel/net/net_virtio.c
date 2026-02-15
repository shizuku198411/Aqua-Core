#include "net.h"
#include "virtio.h"
#include "stdtypes.h"
#include "commonlibs.h"

#define NET_VQ_NUM    8u
#define NET_VQ_ALIGN  4096u
#define NET_VQ_BYTES  (2u * NET_VQ_ALIGN)
#define NET_TXQ_SEL   1u
#define NET_TX_MAX_FRAME 1514u
#define NET_IO_SPIN_LIMIT 30000000u

// Virtio-net header (legacy+modern common layout).
struct virtio_net_hdr {
    uint8_t flags;
    uint8_t gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    uint16_t num_buffers;
} __attribute__((packed));

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
static uint16_t net_tx_last_used_idx;
static int net_ready;
static int net_tx_log_once;
static int net_probe_sent;
static struct virtio_net_hdr net_tx_hdr;
static uint8_t net_tx_frame_buf[NET_TX_MAX_FRAME];
static void net_send_probe_frame_once(void);

static inline void fence_rw_rw(void) {
    __asm__ __volatile__("fence rw, rw" ::: "memory");
}

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
    net_tx_last_used_idx = 0;
    net_ready = 0;

    if (configure_device() < 0) {
        mmio_write(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_FAILED);
        printf("     [net] virtio-net configure failed\n");
        return -1;
    }

    read_mac_addr();
    net_ready = 1;
    printf("     [net] virtio-net init OK:\n           mac=%x:%x:%x:%x:%x:%x\n",
           (unsigned) net_mac[0],
           (unsigned) net_mac[1],
           (unsigned) net_mac[2],
           (unsigned) net_mac[3],
           (unsigned) net_mac[4],
           (unsigned) net_mac[5]);

    net_send_probe_frame_once();
    return 0;
}

int net_tx_frame(const void *buf, size_t len) {
    if (!net_ready) {
        return -2;
    }
    if (!buf || len == 0u) {
        return -1;
    }
    if (len > NET_TX_MAX_FRAME) {
        return -3;
    }

    memcpy(net_tx_frame_buf, buf, len);
    memset(&net_tx_hdr, 0, sizeof(net_tx_hdr));

    // 2-descriptor chain: [virtio_net_hdr] -> [payload]
    net_txq.desc[0].addr = (uint64_t) (uint32_t) &net_tx_hdr;
    net_txq.desc[0].len = (uint32_t) sizeof(net_tx_hdr);
    net_txq.desc[0].flags = VIRTQ_DESC_F_NEXT;
    net_txq.desc[0].next = 1;

    net_txq.desc[1].addr = (uint64_t) (uint32_t) net_tx_frame_buf;
    net_txq.desc[1].len = (uint32_t) len;
    net_txq.desc[1].flags = 0;
    net_txq.desc[1].next = 0;

    uint16_t avail_idx = net_txq.avail->idx;
    net_txq.avail->ring[avail_idx % NET_VQ_NUM] = 0;

    fence_rw_rw();
    net_txq.avail->idx = (uint16_t) (avail_idx + 1);
    fence_rw_rw();
    mmio_write(VIRTIO_MMIO_QUEUE_NOTIFY, NET_TXQ_SEL);

    uint32_t spin = 0;
    while (net_txq.used->idx == net_tx_last_used_idx) {
        __asm__ __volatile__("nop");
        if (++spin > NET_IO_SPIN_LIMIT) {
            if (!net_tx_log_once) {
                printf("[net] tx timeout len=%d used=%d last=%d\n",
                       (int) len,
                       (int) net_txq.used->idx,
                       (int) net_tx_last_used_idx);
                net_tx_log_once = 1;
            }
            return -5;
        }
    }

    net_tx_last_used_idx = net_txq.used->idx;
    fence_rw_rw();
    return 0;
}

static void net_send_probe_frame_once(void) {
    if (net_probe_sent) {
        return;
    }
    net_probe_sent = 1;

    // Ethernet II test frame:
    // dst: broadcast, src: device MAC, ethertype: 0x88B5 (experimental/local use)
    // payload: short ASCII marker (padded to minimum Ethernet payload size)
    uint8_t frame[60];
    memset(frame, 0, sizeof(frame));

    // dst MAC (broadcast)
    for (int i = 0; i < 6; i++) {
        frame[i] = 0xffu;
    }
    // src MAC
    for (int i = 0; i < 6; i++) {
        frame[6 + i] = net_mac[i];
    }
    // EtherType 0x88B5
    frame[12] = 0x88u;
    frame[13] = 0xb5u;

    const char *tag = "aquacore-net-tx-probe";
    int tag_pos = 14;
    for (int i = 0; tag[i] != '\0' && tag_pos < (int) sizeof(frame); i++, tag_pos++) {
        frame[tag_pos] = (uint8_t) tag[i];
    }

    int ret = net_tx_frame(frame, sizeof(frame));
    if (ret < 0) {
        printf("     [net] probe tx failed (%d)\n", ret);
    } else {
        printf("     [net] probe tx sent (ethertype=0x88b5 len=%d)\n", (int) sizeof(frame));
    }
}
