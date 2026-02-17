#include "net/protocol.h"
#include "net/packet.h"
#include "core/commonlibs.h"

static inline void write_be16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t) ((v >> 8) & 0xffu);
    p[1] = (uint8_t) (v & 0xffu);
}

uint16_t net_hton16(uint16_t v) {
    return (uint16_t) ((v << 8) | (v >> 8));
}

uint32_t net_hton32(uint32_t v) {
    return ((v & 0x000000ffu) << 24)
         | ((v & 0x0000ff00u) << 8)
         | ((v & 0x00ff0000u) >> 8)
         | ((v & 0xff000000u) >> 24);
}

uint16_t net_checksum16(const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *) data;
    uint32_t sum = 0;

    while (len >= 2u) {
        sum += ((uint32_t) p[0] << 8) | (uint32_t) p[1];
        p += 2;
        len -= 2;
    }

    if (len == 1u) {
        sum += ((uint32_t) p[0] << 8);
    }

    while ((sum >> 16) != 0u) {
        sum = (sum & 0xffffu) + (sum >> 16);
    }

    return (uint16_t) (~sum & 0xffffu);
}

struct checksum_stream {
    uint32_t sum;
    int has_odd;
    uint8_t odd_hi;
};

static void checksum_stream_feed(struct checksum_stream *st, const uint8_t *p, size_t len) {
    if (!st || !p || len == 0u) {
        return;
    }

    if (st->has_odd) {
        st->sum += ((uint16_t) st->odd_hi << 8) | p[0];
        p++;
        len--;
        st->has_odd = 0;
    }

    while (len >= 2u) {
        st->sum += ((uint16_t) p[0] << 8) | (uint16_t) p[1];
        p += 2;
        len -= 2;
    }

    if (len == 1u) {
        st->odd_hi = p[0];
        st->has_odd = 1;
    }
}

static uint16_t checksum_stream_finalize(struct checksum_stream *st) {
    if (!st) {
        return 0u;
    }
    if (st->has_odd) {
        st->sum += ((uint16_t) st->odd_hi << 8);
        st->has_odd = 0;
    }

    while ((st->sum >> 16) != 0u) {
        st->sum = (st->sum & 0xffffu) + (st->sum >> 16);
    }
    return (uint16_t) (~st->sum & 0xffffu);
}

uint16_t net_ipv4_l4_checksum(
    uint32_t src_ip,
    uint32_t dst_ip,
    uint8_t protocol,
    const void *l4_hdr,
    size_t l4_hdr_len,
    const void *payload,
    size_t payload_len
) {
    if (!l4_hdr || l4_hdr_len == 0u) {
        return 0u;
    }
    if (payload_len > 0u && !payload) {
        return 0u;
    }

    size_t l4_total_len = l4_hdr_len + payload_len;
    if (l4_total_len > 0xffffu) {
        return 0u;
    }

    struct net_ipv4_pseudo_header pse;
    pse.src_ip_be = net_hton32(src_ip);
    pse.dst_ip_be = net_hton32(dst_ip);
    pse.pad = 0u;
    pse.protocol = protocol;
    pse.total_len_be = net_hton16((uint16_t) l4_total_len);

    struct checksum_stream st = {0u, 0, 0u};
    checksum_stream_feed(&st, (const uint8_t *) &pse, sizeof(pse));
    checksum_stream_feed(&st, (const uint8_t *) l4_hdr, l4_hdr_len);
    checksum_stream_feed(&st, (const uint8_t *) payload, payload_len);
    return checksum_stream_finalize(&st);
}

int net_build_ethernet_header(
    uint8_t *out,
    size_t out_cap,
    const uint8_t src_mac[NET_ETH_ADDR_LEN],
    const uint8_t dst_mac[NET_ETH_ADDR_LEN],
    uint16_t eth_type
) {
    if (!out || !src_mac || !dst_mac) {
        return -1;
    }
    if (out_cap < NET_ETH_HDR_LEN) {
        return -2;
    }

    struct net_eth_header *eth = (struct net_eth_header *) out;
    memcpy(eth->dst_mac, dst_mac, NET_ETH_ADDR_LEN);
    memcpy(eth->src_mac, src_mac, NET_ETH_ADDR_LEN);
    eth->eth_type_be = net_hton16(eth_type);
    return (int) NET_ETH_HDR_LEN;
}

int net_build_ipv4_header(
    uint8_t *out,
    size_t out_cap,
    uint16_t total_len,
    uint16_t ident,
    uint16_t flags_frag,
    uint8_t ttl,
    uint8_t protocol,
    uint32_t src_ip,
    uint32_t dst_ip
) {
    if (!out) {
        return -1;
    }
    if (out_cap < NET_IPV4_HDR_LEN) {
        return -2;
    }
    if (total_len < NET_IPV4_HDR_LEN) {
        return -3;
    }

    struct net_ipv4_header *ip = (struct net_ipv4_header *) out;
    ip->version_ihl = NET_IPV4_VERSION_IHL;
    ip->dscp_ecn = 0;
    ip->total_len_be = net_hton16(total_len);
    ip->ident_be = net_hton16(ident);
    ip->flags_frag_be = net_hton16(flags_frag);
    ip->ttl = ttl;
    ip->protocol = protocol;
    ip->hdr_checksum_be = 0;
    ip->src_ip_be = net_hton32(src_ip);
    ip->dst_ip_be = net_hton32(dst_ip);
    ip->hdr_checksum_be = net_hton16(net_checksum16(out, NET_IPV4_HDR_LEN));
    return (int) NET_IPV4_HDR_LEN;
}

int net_build_icmp_echo_request(
    uint8_t *out,
    size_t out_cap,
    const uint8_t src_mac[NET_ETH_ADDR_LEN],
    const uint8_t dst_mac[NET_ETH_ADDR_LEN],
    uint32_t src_ip,
    uint32_t dst_ip,
    uint16_t ident,
    uint16_t seq,
    const void *payload,
    size_t payload_len
) {
    if (!out || !src_mac || !dst_mac) {
        return -1;
    }
    if ((payload_len > 0u) && (payload == NULL)) {
        return -1;
    }

    size_t icmp_len = NET_ICMP_ECHO_HDR_LEN + payload_len;
    size_t ip_len = NET_IPV4_HDR_LEN + icmp_len;
    if (ip_len > 0xffffu) {
        return -2;
    }

    size_t frame_len = NET_ETH_HDR_LEN + ip_len;
    if (frame_len < NET_ETH_FRAME_MIN_LEN) {
        frame_len = NET_ETH_FRAME_MIN_LEN;
    }
    if (frame_len > out_cap) {
        return -3;
    }

    memset(out, 0, frame_len);

    if (net_build_ethernet_header(out,
                                  frame_len,
                                  src_mac,
                                  dst_mac,
                                  NET_ETH_TYPE_IPV4) < 0) {
        return -1;
    }

    uint8_t *ip = &out[NET_ETH_HDR_LEN];
    uint8_t *icmp = ip + NET_IPV4_HDR_LEN;

    // ICMP Echo Request
    icmp[0] = NET_ICMP_TYPE_ECHO_REQ;
    icmp[1] = 0;
    write_be16(&icmp[2], 0);
    write_be16(&icmp[4], ident);
    write_be16(&icmp[6], seq);
    if (payload_len > 0u) {
        memcpy(&icmp[NET_ICMP_ECHO_HDR_LEN], payload, payload_len);
    }
    write_be16(&icmp[2], net_checksum16(icmp, icmp_len));

    if (net_build_ipv4_header(ip,
                              frame_len - NET_ETH_HDR_LEN,
                              (uint16_t) ip_len,
                              0,
                              NET_IPV4_FLAG_DF,
                              NET_IPV4_DEFAULT_TTL,
                              NET_IPV4_PROTO_ICMP,
                              src_ip,
                              dst_ip) < 0) {
        return -1;
    }

    return (int) frame_len;
}
