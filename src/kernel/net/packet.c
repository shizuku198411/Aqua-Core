#include "net/packet.h"
#include "core/commonlibs.h"

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

    memcpy(&out[0], dst_mac, NET_ETH_ADDR_LEN);
    memcpy(&out[6], src_mac, NET_ETH_ADDR_LEN);
    write_be16(&out[12], eth_type);
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

    out[0] = NET_IPV4_VERSION_IHL;
    out[1] = 0;
    write_be16(&out[2], total_len);
    write_be16(&out[4], ident);
    write_be16(&out[6], flags_frag);
    out[8] = ttl;
    out[9] = protocol;
    write_be16(&out[10], 0);
    write_be32(&out[12], src_ip);
    write_be32(&out[16], dst_ip);
    write_be16(&out[10], net_checksum16(out, NET_IPV4_HDR_LEN));
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
