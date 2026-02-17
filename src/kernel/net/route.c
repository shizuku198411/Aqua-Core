#include "net/net.h"

uint32_t net_ipv4_source_addr(void) {
    return NET_IPV4_ADDR;
}

uint32_t net_ipv4_next_hop(uint32_t dst_ip) {
    // Local subnet: direct ARP for destination.
    if ((dst_ip & NET_IPV4_NETMASK) == (NET_IPV4_ADDR & NET_IPV4_NETMASK)) {
        return dst_ip;
    }
    // Remote subnet: send via default gateway.
    return NET_IPV4_GATEWAY;
}
