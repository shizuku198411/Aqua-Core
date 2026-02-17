#pragma once

#include "core/stdtypes.h"

struct net_eth_header {
    uint8_t dst_mac[6];
    uint8_t src_mac[6];
    uint16_t eth_type_be;
} __attribute__((packed));

struct net_ipv4_header {
    uint8_t version_ihl;
    uint8_t dscp_ecn;
    uint16_t total_len_be;
    uint16_t ident_be;
    uint16_t flags_frag_be;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t hdr_checksum_be;
    uint32_t src_ip_be;
    uint32_t dst_ip_be;
} __attribute__((packed));

struct net_ipv4_pseudo_header {
    uint32_t src_ip_be;
    uint32_t dst_ip_be;
    uint8_t pad;        // pad must be zero
    uint8_t protocol;
    uint16_t total_len_be;
} __attribute__((packed));

#define NET_ETH_ADDR_LEN        6u
#define NET_ETH_HDR_LEN         ((size_t) sizeof(struct net_eth_header))
#define NET_ETH_TYPE_IPV4       0x0800u
#define NET_ETH_TYPE_ARP        0x0806u
#define NET_ETH_FRAME_MIN_LEN   60u

#define NET_IPV4_HDR_LEN        ((size_t) sizeof(struct net_ipv4_header))
#define NET_IPV4_VERSION_IHL    0x45u
#define NET_IPV4_FLAG_DF        0x4000u
#define NET_IPV4_DEFAULT_TTL    64u

#define NET_ICMP_ECHO_HDR_LEN   8u
#define NET_ICMP_TYPE_ECHO_REQ  8u
#define NET_ICMP_TYPE_ECHO_REP  0u

uint16_t net_hton16(uint16_t v);
uint32_t net_hton32(uint32_t v);

#define net_ntoh16(v) net_hton16(v)
#define net_ntoh32(v) net_hton32(v)

uint16_t net_checksum16(const void *data, size_t len);
uint16_t net_ipv4_l4_checksum(
    uint32_t src_ip,
    uint32_t dst_ip,
    uint8_t protocol,
    const void *l4_hdr,
    size_t l4_hdr_len,
    const void *payload,
    size_t payload_len
);

int net_build_ethernet_header(
    uint8_t *out,
    size_t out_cap,
    const uint8_t src_mac[NET_ETH_ADDR_LEN],
    const uint8_t dst_mac[NET_ETH_ADDR_LEN],
    uint16_t eth_type
);

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
);

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
);
