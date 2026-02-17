#pragma once

// MMIO transport base and slot layout (QEMU virt machine).
#define VIRTIO_MMIO_BASE       0x10001000u
#define VIRTIO_MMIO_STRIDE     0x1000u
#define VIRTIO_MMIO_MAX_DEVS   8u

// Common virtio-mmio register offsets.
#define VIRTIO_MMIO_MAGIC_VALUE       0x000u
#define VIRTIO_MMIO_VERSION           0x004u
#define VIRTIO_MMIO_DEVICE_ID         0x008u
#define VIRTIO_MMIO_VENDOR_ID         0x00cu
#define VIRTIO_MMIO_DEVICE_FEATURES       0x010u
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL   0x014u
#define VIRTIO_MMIO_DRIVER_FEATURES       0x020u
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL   0x024u
#define VIRTIO_MMIO_QUEUE_SEL         0x030u
#define VIRTIO_MMIO_QUEUE_NUM_MAX     0x034u
#define VIRTIO_MMIO_QUEUE_NUM         0x038u
#define VIRTIO_MMIO_QUEUE_READY       0x044u
#define VIRTIO_MMIO_QUEUE_NOTIFY      0x050u
#define VIRTIO_MMIO_STATUS            0x070u
#define VIRTIO_MMIO_QUEUE_DESC_LOW    0x080u
#define VIRTIO_MMIO_QUEUE_DESC_HIGH   0x084u
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW   0x090u
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH  0x094u
#define VIRTIO_MMIO_QUEUE_USED_LOW    0x0a0u
#define VIRTIO_MMIO_QUEUE_USED_HIGH   0x0a4u
#define VIRTIO_MMIO_CONFIG            0x100u

// Common IDs.
#define VIRTIO_MAGIC      0x74726976u
#define VIRTIO_VENDOR     0x554d4551u
#define VIRTIO_DEV_NET    1u
#define VIRTIO_DEV_BLOCK  2u

// Common status bits.
#define VIRTIO_STATUS_ACKNOWLEDGE  1u
#define VIRTIO_STATUS_DRIVER       2u
#define VIRTIO_STATUS_DRIVER_OK    4u
#define VIRTIO_STATUS_FEATURES_OK  8u
#define VIRTIO_STATUS_FAILED       128u

// Common virtqueue descriptor flags.
#define VIRTQ_DESC_F_NEXT   1u
#define VIRTQ_DESC_F_WRITE  2u

// Common virtio feature bits.
#define VIRTIO_F_ANY_LAYOUT          27u
#define VIRTIO_RING_F_INDIRECT_DESC  28u
#define VIRTIO_RING_F_EVENT_IDX      29u
#define VIRTIO_F_VERSION_1           32u

// Virtio-net feature bits (device features sel=0).
#define VIRTIO_NET_F_MAC             5u
