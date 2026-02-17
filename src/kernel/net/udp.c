#include "net/protocol.h"
#include "net/packet.h"
#include "net/udp.h"
#include "net/net.h"
#include "net/arp.h"
#include "core/commonlibs.h"

int net_build_udp_header(
    uint8_t *out,
    size_t out_cap,
    uint32_t src_ip,
    uint32_t dst_ip,
    uint16_t sport,
    uint16_t dport,
    const void *payload,
    size_t payload_len
) {
    if (!out) {
        return -1;
    }
    if (payload_len > 0u && !payload) {
        return -1;
    }
    if (out_cap < NET_UDP_HDR_LEN) {
        return -2;
    }

    size_t udp_len = NET_UDP_HDR_LEN + payload_len;
    if (udp_len > 0xffffu) {
        return -3;
    }

    struct net_udp_header *uh = (struct net_udp_header *) out;
    uh->uh_sport = net_hton16(sport);
    uh->uh_dport = net_hton16(dport);
    uh->uh_ulen = net_hton16((uint16_t) udp_len);
    uh->uh_sum = 0;

    uint16_t csum = net_ipv4_l4_checksum(src_ip,
                                         dst_ip,
                                         NET_IPV4_PROTO_UDP,
                                         uh,
                                         NET_UDP_HDR_LEN,
                                         payload,
                                         payload_len);
    // RFC 768: computed checksum 0 is transmitted as 0xffff.
    uh->uh_sum = net_hton16((csum == 0u) ? 0xffffu : csum);
    return (int) NET_UDP_HDR_LEN;
}

int net_udp_send_once(uint32_t dst_ip, uint16_t sport, uint16_t dport, const void *payload, size_t len) {
    if (dst_ip == 0u || dst_ip == 0xffffffffu) {
        return NET_ERR_INVAL;
    }
    if (len > 0u && !payload) {
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

    size_t ip_payload_len = NET_UDP_HDR_LEN + len;
    size_t ip_total_len = NET_IPV4_HDR_LEN + ip_payload_len;
    if (ip_total_len > 0xffffu) {
        return NET_ERR_TOO_LARGE;
    }

    size_t frame_len = NET_ETH_HDR_LEN + ip_total_len;
    if (frame_len < NET_ETH_FRAME_MIN_LEN) {
        frame_len = NET_ETH_FRAME_MIN_LEN;
    }
    if (frame_len > NET_UDP_FRAME_CAP) {
        return NET_ERR_TOO_LARGE;
    }

    uint8_t frame[NET_UDP_FRAME_CAP];
    memset(frame, 0, frame_len);

    if (net_build_ethernet_header(frame,
                                  frame_len,
                                  src_mac,
                                  dst_mac,
                                  NET_ETH_TYPE_IPV4) < 0) {
        return NET_ERR_BUILD_FAILED;
    }

    uint8_t *ip = &frame[NET_ETH_HDR_LEN];
    uint8_t *udp = ip + NET_IPV4_HDR_LEN;
    uint8_t *udp_payload = udp + NET_UDP_HDR_LEN;

    if (len > 0u) {
        memcpy(udp_payload, payload, len);
    }

    if (net_build_udp_header(udp,
                             frame_len - NET_ETH_HDR_LEN - NET_IPV4_HDR_LEN,
                             src_ip,
                             dst_ip,
                             sport,
                             dport,
                             payload,
                             len) < 0) {
        return NET_ERR_BUILD_FAILED;
    }

    if (net_build_ipv4_header(ip,
                              frame_len - NET_ETH_HDR_LEN,
                              (uint16_t) ip_total_len,
                              0,
                              NET_IPV4_FLAG_DF,
                              NET_IPV4_DEFAULT_TTL,
                              NET_IPV4_PROTO_UDP,
                              src_ip,
                              dst_ip) < 0) {
        return NET_ERR_BUILD_FAILED;
    }

    return net_tx_frame(frame, frame_len);
}
