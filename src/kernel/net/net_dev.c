#include "net/net.h"
#include "net/udp.h"
#include "core/commonlibs.h"

#define NET_DEV_PING_MAGIC  0x50494e47u  // "PING"
#define NET_DEV_UDP0_MAGIC  0x55445030u  // "UDP0"

static uint16_t read_be16(const uint8_t *p) {
    return (uint16_t) (((uint16_t) p[0] << 8) | (uint16_t) p[1]);
}

static uint32_t read_be32(const uint8_t *p) {
    return ((uint32_t) p[0] << 24)
         | ((uint32_t) p[1] << 16)
         | ((uint32_t) p[2] << 8)
         | (uint32_t) p[3];
}

int net_dev_read(void *buf, size_t size) {
    if (!buf) {
        return NET_ERR_INVAL;
    }
    if (size == 0u) {
        return 0;
    }

    const uint8_t *frame = NULL;
    size_t frame_len = 0;
    int ret = net_rx_try_dequeue(&frame, &frame_len);
    if (ret < 0) {
        return ret;
    }

    size_t copy_len = (frame_len < size) ? frame_len : size;
    memcpy(buf, frame, copy_len);
    return (int) copy_len;
}

int net_dev_write(const void *buf, size_t size) {
    if (!buf) {
        return NET_ERR_INVAL;
    }
    if (size == 0u) {
        return 0;
    }

    const uint8_t *p = (const uint8_t *) buf;
    if (size >= 4u) {
        uint32_t magic = read_be32(p);

        // PING request
        // Layout: [magic:"PING"(32), dst_ip(32), id(16), seq(16)] (12 bytes)
        if (magic == NET_DEV_PING_MAGIC && size == 12u) {
            uint32_t dst_ip = read_be32(&p[4]);
            uint16_t id = read_be16(&p[8]);
            uint16_t seq = read_be16(&p[10]);
            return net_ping_send_once(dst_ip, id, seq);
        }

        // UDP send request
        // Layout: [magic:"UDP0"(32), dst_ip(32), sport(16), dport(16), payload...]
        if (magic == NET_DEV_UDP0_MAGIC && size >= 12u) {
            uint32_t dst_ip = read_be32(&p[4]);
            uint16_t sport = read_be16(&p[8]);
            uint16_t dport = read_be16(&p[10]);
            const uint8_t *payload = &p[12];
            size_t payload_len = size - 12u;
            return net_udp_send_once(dst_ip, sport, dport, payload, payload_len);
        }
    }

    // Raw frame send fallback.
    return net_tx_frame(buf, size);
}
