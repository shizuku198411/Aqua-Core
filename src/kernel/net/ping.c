#include "net/net.h"
#include "net/packet.h"
#include "net/arp.h"
#include "core/commonlibs.h"

#define NET_PING_FRAME_CAP  1514u
#define NET_PING_PAYLOAD    "aquacore-ping"
#define NET_PING_RX_POLL_LIMIT  3000000u
#define NET_DEV_PING_MAGIC  0x50494e47u  // "PING"

static uint16_t read_be16(const uint8_t *p) {
    return (uint16_t) (((uint16_t) p[0] << 8) | (uint16_t) p[1]);
}

static uint32_t read_be32(const uint8_t *p) {
    return ((uint32_t) p[0] << 24)
         | ((uint32_t) p[1] << 16)
         | ((uint32_t) p[2] << 8)
         | (uint32_t) p[3];
}

static int match_icmp_echo_reply(const uint8_t *frame,
                                 size_t frame_len,
                                 uint32_t expect_src_ip,
                                 uint32_t expect_dst_ip,
                                 uint16_t expect_id,
                                 uint16_t expect_seq) {
    if (!frame || frame_len < (NET_ETH_HDR_LEN + NET_IPV4_HDR_LEN + NET_ICMP_ECHO_HDR_LEN)) {
        return 0;
    }

    // Ethernet type must be IPv4.
    if (read_be16(&frame[12]) != NET_ETH_TYPE_IPV4) {
        return 0;
    }

    const uint8_t *ip = &frame[NET_ETH_HDR_LEN];
    uint8_t version = (uint8_t) (ip[0] >> 4);
    uint8_t ihl_words = (uint8_t) (ip[0] & 0x0fu);
    size_t ip_hdr_len = (size_t) ihl_words * 4u;
    if (version != 4u || ihl_words < 5u) {
        return 0;
    }
    if (frame_len < NET_ETH_HDR_LEN + ip_hdr_len + NET_ICMP_ECHO_HDR_LEN) {
        return 0;
    }
    if (ip[9] != NET_IPV4_PROTO_ICMP) {
        return 0;
    }

    uint16_t ip_total_len = read_be16(&ip[2]);
    if (ip_total_len < ip_hdr_len + NET_ICMP_ECHO_HDR_LEN) {
        return 0;
    }
    if (ip_total_len > frame_len - NET_ETH_HDR_LEN) {
        return 0;
    }

    uint32_t src_ip = read_be32(&ip[12]);
    uint32_t dst_ip = read_be32(&ip[16]);
    if (src_ip != expect_src_ip || dst_ip != expect_dst_ip) {
        return 0;
    }

    const uint8_t *icmp = ip + ip_hdr_len;
    if (icmp[0] != NET_ICMP_TYPE_ECHO_REP || icmp[1] != 0u) {
        return 0;
    }

    uint16_t id = read_be16(&icmp[4]);
    uint16_t seq = read_be16(&icmp[6]);
    if (id != expect_id || seq != expect_seq) {
        return 0;
    }

    return 1;
}

int net_ping_send_once(uint32_t dst_ip, uint16_t id, uint16_t seq) {
    if (dst_ip == 0u || dst_ip == 0xffffffffu) {
        return NET_ERR_INVAL;
    }

    uint32_t src_ip = net_ipv4_source_addr();
    uint32_t next_hop = net_ipv4_next_hop(dst_ip);

    uint8_t src_mac[NET_ETH_ADDR_LEN];
    int mac_ret = net_get_mac(src_mac);
    if (mac_ret < 0) {
        return mac_ret;
    }

    uint8_t dst_mac[NET_ETH_ADDR_LEN];
    int arp_ret = net_arp_resolve(src_ip, next_hop, dst_mac);
    if (arp_ret < 0) {
        return arp_ret;
    }
    static const uint8_t payload[] = NET_PING_PAYLOAD;
    uint8_t frame[NET_PING_FRAME_CAP];

    int frame_len = net_build_icmp_echo_request(frame,
                                                sizeof(frame),
                                                src_mac,
                                                dst_mac,
                                                src_ip,
                                                dst_ip,
                                                id,
                                                seq,
                                                payload,
                                                sizeof(payload) - 1u);
    if (frame_len < 0) {
        return NET_ERR_BUILD_FAILED;
    }

    int tx_ret = net_tx_frame(frame, (size_t) frame_len);
    if (tx_ret < 0) {
        return tx_ret;
    }

    // Wait until a matching Echo Reply arrives or timeout.
    uint32_t poll = 0;
    while (poll++ < NET_PING_RX_POLL_LIMIT) {
        const uint8_t *rx_frame = NULL;
        size_t rx_len = 0;
        int rx_ret = net_rx_try_dequeue(&rx_frame, &rx_len);
        if (rx_ret == 0) {
            if (match_icmp_echo_reply(rx_frame, rx_len, dst_ip, src_ip, id, seq)) {
                return 0;
            }
            continue;
        }
        if (rx_ret < 0 && rx_ret != -1) {
            return rx_ret;
        }
        __asm__ __volatile__("nop");
    }

    return NET_ERR_PING_TIMEOUT;
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

    // Generic netdev command envelope (current op: ping request).
    // Layout: [magic:"PING"(32), dst_ip(32), id(16), seq(16)].
    if (size == 12u) {
        const uint8_t *p = (const uint8_t *) buf;
        uint32_t magic = ((uint32_t) p[0] << 24)
                       | ((uint32_t) p[1] << 16)
                       | ((uint32_t) p[2] << 8)
                       | (uint32_t) p[3];
        if (magic == NET_DEV_PING_MAGIC) {
            uint32_t dst_ip = ((uint32_t) p[4] << 24)
                            | ((uint32_t) p[5] << 16)
                            | ((uint32_t) p[6] << 8)
                            | (uint32_t) p[7];
            uint16_t id = (uint16_t) (((uint16_t) p[8] << 8) | (uint16_t) p[9]);
            uint16_t seq = (uint16_t) (((uint16_t) p[10] << 8) | (uint16_t) p[11]);
            return net_ping_send_once(dst_ip, id, seq);
        }
    }

    return net_tx_frame(buf, size);
}
