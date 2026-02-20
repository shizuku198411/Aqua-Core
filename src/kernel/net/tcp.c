#include "net/tcp.h"
#include "core/commonlibs.h"

#define NET_TCP_CONN_MAX 16

struct net_tcp_conn {
    int used;
    struct net_tcp_endpoint ep;
    enum net_tcp_state state;
};

static struct net_tcp_conn tcp_conns[NET_TCP_CONN_MAX];

void net_tcp_init(void) {
    memset(tcp_conns, 0, sizeof(tcp_conns));
}

int net_tcp_alloc(struct net_tcp_endpoint ep) {
    for (int i = 0; i < NET_TCP_CONN_MAX; i++) {
        if (!tcp_conns[i].used) {
            tcp_conns[i].used = 1;
            tcp_conns[i].ep = ep;
            tcp_conns[i].state = NET_TCP_CLOSED;
            return i;
        }
    }
    return -1;
}

int net_tcp_set_state(int id, enum net_tcp_state state) {
    if (id < 0 || id >= NET_TCP_CONN_MAX || !tcp_conns[id].used) {
        return -1;
    }
    tcp_conns[id].state = state;
    return 0;
}

enum net_tcp_state net_tcp_get_state(int id) {
    if (id < 0 || id >= NET_TCP_CONN_MAX || !tcp_conns[id].used) {
        return NET_TCP_CLOSED;
    }
    return tcp_conns[id].state;
}

const char *net_tcp_state_name(enum net_tcp_state state) {
    switch (state) {
        case NET_TCP_CLOSED: return "CLOSED";
        case NET_TCP_LISTEN: return "LISTEN";
        case NET_TCP_SYN_SENT: return "SYN_SENT";
        case NET_TCP_SYN_RECV: return "SYN_RECV";
        case NET_TCP_ESTABLISHED: return "ESTABLISHED";
        case NET_TCP_FIN_WAIT_1: return "FIN_WAIT_1";
        case NET_TCP_FIN_WAIT_2: return "FIN_WAIT_2";
        case NET_TCP_CLOSE_WAIT: return "CLOSE_WAIT";
        case NET_TCP_CLOSING: return "CLOSING";
        case NET_TCP_LAST_ACK: return "LAST_ACK";
        case NET_TCP_TIME_WAIT: return "TIME_WAIT";
        default: return "UNKNOWN";
    }
}
