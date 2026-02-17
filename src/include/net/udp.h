#pragma once

#include "core/stdtypes.h"

struct net_udp_header {
    uint16_t uh_sport;      // source port
    uint16_t uh_dport;      // dest port
    uint16_t uh_ulen;       // udp length
    uint16_t uh_sum;        // checksum
} __attribute__((packed));

#define NET_UDP_HDR_LEN ((size_t) sizeof(struct net_udp_header))
#define NET_UDP_FRAME_CAP 1514u

int net_build_udp_header(
    uint8_t *out,
    size_t out_cap,
    uint32_t src_ip,
    uint32_t dst_ip,
    uint16_t sport,
    uint16_t dport,
    const void *payload,
    size_t payload_len
);

int net_udp_send_once(uint32_t dst_ip,
                      uint16_t sport,
                      uint16_t dport,
                      const void *payload,
                      size_t len);
