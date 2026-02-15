#include "net.h"
#include "net_packet.h"
#include "net_arp.h"
#include "commonlibs.h"

#define NET_ARP_FRAME_LEN        60u
#define NET_ARP_POLL_LIMIT       50000000u
#define NET_ARP_RETRY_MAX        3u
#define NET_ARP_HTYPE_ETHERNET   1u
#define NET_ARP_PTYPE_IPV4       0x0800u
#define NET_ARP_OP_REQUEST       1u
#define NET_ARP_OP_REPLY         2u

static inline void write_be16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t) ((v >> 8) & 0xffu);
    p[1] = (uint8_t) (v & 0xffu);
}

static inline void write_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t) ((v >> 24) & 0xffu);
    p[1] = (uint8_t) ((v >> 16) & 0xffu);
    p[2] = (uint8_t) ((v >> 8) & 0xffu);
    p[3] = (uint8_t) (v & 0xffu);
}

static inline uint16_t read_be16(const uint8_t *p) {
    return (uint16_t) (((uint16_t) p[0] << 8) | (uint16_t) p[1]);
}

static inline uint32_t read_be32(const uint8_t *p) {
    return ((uint32_t) p[0] << 24)
         | ((uint32_t) p[1] << 16)
         | ((uint32_t) p[2] << 8)
         | (uint32_t) p[3];
}

static void build_arp_request(uint8_t frame[NET_ARP_FRAME_LEN],
                              const uint8_t src_mac[6],
                              uint32_t src_ip,
                              uint32_t target_ip) {
    memset(frame, 0, NET_ARP_FRAME_LEN);

    // Ethernet header
    for (int i = 0; i < 6; i++) {
        frame[i] = 0xffu;         // dst: broadcast
        frame[6 + i] = src_mac[i]; // src
    }
    write_be16(&frame[12], NET_ETH_TYPE_ARP);

    // ARP payload (Ethernet + IPv4)
    uint8_t *arp = &frame[14];
    write_be16(&arp[0], NET_ARP_HTYPE_ETHERNET);
    write_be16(&arp[2], NET_ARP_PTYPE_IPV4);
    arp[4] = 6; // hlen
    arp[5] = 4; // plen
    write_be16(&arp[6], NET_ARP_OP_REQUEST);
    memcpy(&arp[8], src_mac, 6);   // sender hardware addr
    write_be32(&arp[14], src_ip);  // sender protocol addr
    memset(&arp[18], 0, 6);        // target hardware addr unknown
    write_be32(&arp[24], target_ip); // target protocol addr
}

static int parse_arp_reply(const uint8_t *frame,
                           size_t len,
                           uint32_t expect_spa,
                           uint32_t expect_tpa,
                           uint8_t out_mac[6]) {
    if (!frame || len < 42u || !out_mac) {
        return 0;
    }

    if (read_be16(&frame[12]) != NET_ETH_TYPE_ARP) {
        return 0;
    }

    const uint8_t *arp = &frame[14];
    if (read_be16(&arp[0]) != NET_ARP_HTYPE_ETHERNET) {
        return 0;
    }
    if (read_be16(&arp[2]) != NET_ARP_PTYPE_IPV4) {
        return 0;
    }
    if (arp[4] != 6 || arp[5] != 4) {
        return 0;
    }
    if (read_be16(&arp[6]) != NET_ARP_OP_REPLY) {
        return 0;
    }

    uint32_t spa = read_be32(&arp[14]);
    uint32_t tpa = read_be32(&arp[24]);
    if (spa != expect_spa) {
        return 0;
    }
    (void) expect_tpa;
    (void) tpa;

    memcpy(out_mac, &arp[8], 6);
    return 1;
}

int net_arp_resolve(uint32_t src_ip, uint32_t target_ip, uint8_t out_mac[6]) {
    if (!out_mac || src_ip == 0u || target_ip == 0u) {
        return NET_ERR_INVAL;
    }

    uint8_t src_mac[6];
    int ret = net_get_mac(src_mac);
    if (ret < 0) {
        return ret;
    }

    uint8_t req[NET_ARP_FRAME_LEN];
    build_arp_request(req, src_mac, src_ip, target_ip);

    for (uint32_t attempt = 0; attempt < NET_ARP_RETRY_MAX; attempt++) {
        ret = net_tx_frame(req, sizeof(req));
        if (ret < 0) {
            return ret;
        }

        uint32_t poll = 0;
        while (poll++ < NET_ARP_POLL_LIMIT) {
            const uint8_t *rx_frame = NULL;
            size_t rx_len = 0;
            int rx_ret = net_rx_try_dequeue(&rx_frame, &rx_len);
            if (rx_ret == 0) {
                if (parse_arp_reply(rx_frame, rx_len, target_ip, src_ip, out_mac)) {
                    return 0;
                }
                continue;
            }
            if (rx_ret < 0 && rx_ret != -1) {
                return rx_ret;
            }
            __asm__ __volatile__("nop");
        }
    }

    return NET_ERR_ARP_TIMEOUT;
}
