#pragma once

#include "core/stdtypes.h"

enum net_tcp_state {
    NET_TCP_CLOSED = 0,
    NET_TCP_LISTEN,
    NET_TCP_SYN_SENT,
    NET_TCP_SYN_RECV,
    NET_TCP_ESTABLISHED,
    NET_TCP_FIN_WAIT_1,
    NET_TCP_FIN_WAIT_2,
    NET_TCP_CLOSE_WAIT,
    NET_TCP_CLOSING,
    NET_TCP_LAST_ACK,
    NET_TCP_TIME_WAIT,
};

struct net_tcp_endpoint {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
};

void net_tcp_init(void);
int net_tcp_alloc(struct net_tcp_endpoint ep);
int net_tcp_set_state(int id, enum net_tcp_state state);
enum net_tcp_state net_tcp_get_state(int id);
const char *net_tcp_state_name(enum net_tcp_state state);
