#include "net/tcp.h"
#include "net/net.h"
#include "net/arp.h"
#include "net/packet.h"
#include "net/protocol.h"
#include "core/commonlibs.h"

#define NET_TCP_CONN_MAX       16
#define NET_TCP_FRAME_CAP      1514u
#define NET_TCP_HDR_LEN        20u
#define NET_TCP_POLL_LIMIT     2000000u
#define NET_TCP_ACCEPT_POLL_LIMIT 4096u
#define NET_TCP_PAYLOAD_MAX    1200u
#define NET_TCP_PENDING_MAX    4096u

#define TCP_FLAG_FIN 0x01u
#define TCP_FLAG_SYN 0x02u
#define TCP_FLAG_RST 0x04u
#define TCP_FLAG_PSH 0x08u
#define TCP_FLAG_ACK 0x10u

struct net_tcp_header {
    uint16_t src_port_be;
    uint16_t dst_port_be;
    uint32_t seq_be;
    uint32_t ack_be;
    uint8_t data_offset_ns;
    uint8_t flags;
    uint16_t window_be;
    uint16_t checksum_be;
    uint16_t urgent_be;
} __attribute__((packed));

struct net_tcp_segment {
    uint32_t src_ip;
    uint16_t src_port;
    uint32_t dst_ip;
    uint16_t dst_port;
    uint8_t flags;
    uint32_t seq;
    uint32_t ack;
    const uint8_t *payload;
    size_t payload_len;
};

struct net_tcp_conn {
    int used;
    struct net_tcp_endpoint ep;
    enum net_tcp_state state;
    uint32_t send_next;
    uint32_t recv_next;
    uint8_t pending[NET_TCP_PENDING_MAX];
    size_t pending_len;
    size_t pending_off;
};

static struct net_tcp_conn tcp_conns[NET_TCP_CONN_MAX];
static uint32_t tcp_iss_seed = 0x11002345u;

static uint16_t read_be16(const uint8_t *p) {
    return (uint16_t) (((uint16_t) p[0] << 8) | (uint16_t) p[1]);
}

static uint32_t read_be32(const uint8_t *p) {
    return ((uint32_t) p[0] << 24)
         | ((uint32_t) p[1] << 16)
         | ((uint32_t) p[2] << 8)
         | (uint32_t) p[3];
}

static void write_be16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t) ((v >> 8) & 0xffu);
    p[1] = (uint8_t) (v & 0xffu);
}

static void write_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t) ((v >> 24) & 0xffu);
    p[1] = (uint8_t) ((v >> 16) & 0xffu);
    p[2] = (uint8_t) ((v >> 8) & 0xffu);
    p[3] = (uint8_t) (v & 0xffu);
}

static int parse_tcp_segment(const uint8_t *frame, size_t frame_len, struct net_tcp_segment *out) {
    if (!frame || !out) {
        return -1;
    }
    if (frame_len < 14u + 20u + NET_TCP_HDR_LEN) {
        return -1;
    }
    if (read_be16(&frame[12]) != NET_ETH_TYPE_IPV4) {
        return -1;
    }

    const uint8_t *ip = &frame[14];
    if ((ip[0] >> 4) != 4u) {
        return -1;
    }
    uint8_t ihl_words = (uint8_t) (ip[0] & 0x0fu);
    size_t ihl = (size_t) ihl_words * 4u;
    if (ihl < 20u || frame_len < 14u + ihl + NET_TCP_HDR_LEN) {
        return -1;
    }
    if (ip[9] != NET_IPV4_PROTO_TCP) {
        return -1;
    }

    uint16_t total_len = read_be16(&ip[2]);
    if (total_len < ihl + NET_TCP_HDR_LEN) {
        return -1;
    }
    if ((size_t) total_len > frame_len - 14u) {
        return -1;
    }

    const uint8_t *tcp = ip + ihl;
    uint8_t doff_words = (uint8_t) (tcp[12] >> 4);
    size_t tcp_hlen = (size_t) doff_words * 4u;
    if (tcp_hlen < NET_TCP_HDR_LEN) {
        return -1;
    }
    if (ihl + tcp_hlen > total_len) {
        return -1;
    }

    out->src_ip = read_be32(&ip[12]);
    out->dst_ip = read_be32(&ip[16]);
    out->src_port = read_be16(&tcp[0]);
    out->dst_port = read_be16(&tcp[2]);
    out->seq = read_be32(&tcp[4]);
    out->ack = read_be32(&tcp[8]);
    out->flags = tcp[13];
    out->payload = tcp + tcp_hlen;
    out->payload_len = (size_t) total_len - ihl - tcp_hlen;
    return 0;
}

static int tx_tcp_segment(uint32_t src_ip,
                          uint32_t dst_ip,
                          uint16_t src_port,
                          uint16_t dst_port,
                          uint32_t seq,
                          uint32_t ack,
                          uint8_t flags,
                          const void *payload,
                          size_t payload_len) {
    if (payload_len > 0u && !payload) {
        return NET_ERR_INVAL;
    }
    if (payload_len > NET_TCP_PAYLOAD_MAX) {
        return NET_ERR_TOO_LARGE;
    }

    uint32_t next_hop = net_ipv4_next_hop(dst_ip);
    uint8_t src_mac[NET_ETH_ADDR_LEN];
    if (net_get_mac(src_mac) < 0) {
        return NET_ERR_NO_DEVICE;
    }
    uint8_t dst_mac[NET_ETH_ADDR_LEN];
    if (net_arp_resolve(src_ip, next_hop, dst_mac) < 0) {
        return NET_ERR_ARP_TIMEOUT;
    }

    size_t ip_payload_len = NET_TCP_HDR_LEN + payload_len;
    size_t ip_total_len = NET_IPV4_HDR_LEN + ip_payload_len;
    size_t frame_len = NET_ETH_HDR_LEN + ip_total_len;
    if (frame_len < NET_ETH_FRAME_MIN_LEN) {
        frame_len = NET_ETH_FRAME_MIN_LEN;
    }
    if (frame_len > NET_TCP_FRAME_CAP) {
        return NET_ERR_TOO_LARGE;
    }

    uint8_t frame[NET_TCP_FRAME_CAP];
    memset(frame, 0, frame_len);

    if (net_build_ethernet_header(frame, frame_len, src_mac, dst_mac, NET_ETH_TYPE_IPV4) < 0) {
        return NET_ERR_BUILD_FAILED;
    }

    uint8_t *ip = &frame[NET_ETH_HDR_LEN];
    uint8_t *tcp = ip + NET_IPV4_HDR_LEN;
    uint8_t *tcp_payload = tcp + NET_TCP_HDR_LEN;

    write_be16(&tcp[0], src_port);
    write_be16(&tcp[2], dst_port);
    write_be32(&tcp[4], seq);
    write_be32(&tcp[8], ack);
    tcp[12] = 0x50; // data offset=5
    tcp[13] = flags;
    write_be16(&tcp[14], 4096u);
    write_be16(&tcp[16], 0u);
    write_be16(&tcp[18], 0u);

    if (payload_len > 0u) {
        memcpy(tcp_payload, payload, payload_len);
    }

    uint16_t csum = net_ipv4_l4_checksum(src_ip,
                                         dst_ip,
                                         NET_IPV4_PROTO_TCP,
                                         tcp,
                                         NET_TCP_HDR_LEN,
                                         payload,
                                         payload_len);
    write_be16(&tcp[16], csum);

    if (net_build_ipv4_header(ip,
                              frame_len - NET_ETH_HDR_LEN,
                              (uint16_t) ip_total_len,
                              0,
                              NET_IPV4_FLAG_DF,
                              NET_IPV4_DEFAULT_TTL,
                              NET_IPV4_PROTO_TCP,
                              src_ip,
                              dst_ip) < 0) {
        return NET_ERR_BUILD_FAILED;
    }

    return net_tx_frame(frame, frame_len);
}

static int poll_matching_segment(struct net_tcp_conn *c, struct net_tcp_segment *seg_out) {
    if (!c || !seg_out) {
        return -1;
    }

    for (uint32_t spin = 0; spin < NET_TCP_POLL_LIMIT; spin++) {
        const uint8_t *frame = NULL;
        size_t frame_len = 0;
        int rr = net_rx_try_dequeue(&frame, &frame_len);
        if (rr < 0) {
            if (rr != -1) {
                return -1;
            }
            continue;
        }

        struct net_tcp_segment seg;
        if (parse_tcp_segment(frame, frame_len, &seg) < 0) {
            continue;
        }
        if (seg.src_ip != c->ep.dst_ip || seg.dst_ip != c->ep.src_ip) {
            continue;
        }
        if (seg.src_port != c->ep.dst_port || seg.dst_port != c->ep.src_port) {
            continue;
        }

        *seg_out = seg;
        return 0;
    }

    return -1;
}

void net_tcp_init(void) {
    memset(tcp_conns, 0, sizeof(tcp_conns));
}

int net_tcp_alloc(struct net_tcp_endpoint ep) {
    for (int i = 0; i < NET_TCP_CONN_MAX; i++) {
        if (!tcp_conns[i].used) {
            tcp_conns[i].used = 1;
            tcp_conns[i].ep = ep;
            tcp_conns[i].state = NET_TCP_CLOSED;
            tcp_conns[i].send_next = 0;
            tcp_conns[i].recv_next = 0;
            tcp_conns[i].pending_len = 0;
            tcp_conns[i].pending_off = 0;
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

int net_tcp_connect(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port) {
    if (dst_ip == 0u || src_port == 0u || dst_port == 0u) {
        return -1;
    }

    struct net_tcp_endpoint ep;
    ep.src_ip = net_ipv4_source_addr();
    ep.dst_ip = dst_ip;
    ep.src_port = src_port;
    ep.dst_port = dst_port;

    int id = net_tcp_alloc(ep);
    if (id < 0) {
        return -1;
    }

    struct net_tcp_conn *c = &tcp_conns[id];
    uint32_t iss = tcp_iss_seed;
    tcp_iss_seed += 0x10001u;

    c->send_next = iss;
    c->recv_next = 0;
    c->state = NET_TCP_SYN_SENT;

    if (tx_tcp_segment(c->ep.src_ip,
                       c->ep.dst_ip,
                       c->ep.src_port,
                       c->ep.dst_port,
                       c->send_next,
                       0,
                       TCP_FLAG_SYN,
                       NULL,
                       0) < 0) {
        c->used = 0;
        return -1;
    }
    c->send_next += 1u;

    struct net_tcp_segment seg;
    if (poll_matching_segment(c, &seg) < 0) {
        c->used = 0;
        return -1;
    }
    if ((seg.flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) != (TCP_FLAG_SYN | TCP_FLAG_ACK)) {
        c->used = 0;
        return -1;
    }
    if (seg.ack != c->send_next) {
        c->used = 0;
        return -1;
    }

    c->recv_next = seg.seq + 1u;
    if (tx_tcp_segment(c->ep.src_ip,
                       c->ep.dst_ip,
                       c->ep.src_port,
                       c->ep.dst_port,
                       c->send_next,
                       c->recv_next,
                       TCP_FLAG_ACK,
                       NULL,
                       0) < 0) {
        c->used = 0;
        return -1;
    }

    c->state = NET_TCP_ESTABLISHED;
    return id;
}

int net_tcp_accept(uint16_t local_port, uint32_t *peer_ip_out, uint16_t *peer_port_out) {
    if (local_port == 0u) {
        return -1;
    }

    uint32_t local_ip = net_ipv4_source_addr();

    for (uint32_t spin = 0; spin < NET_TCP_ACCEPT_POLL_LIMIT; spin++) {
        const uint8_t *frame = NULL;
        size_t frame_len = 0;
        int rr = net_rx_try_dequeue(&frame, &frame_len);
        if (rr < 0) {
            if (rr != -1) {
                return -1;
            }
            continue;
        }

        struct net_tcp_segment syn;
        if (parse_tcp_segment(frame, frame_len, &syn) < 0) {
            continue;
        }
        if (syn.dst_ip != local_ip) {
            continue;
        }
        if (syn.dst_port != local_port) {
            continue;
        }
        if ((syn.flags & TCP_FLAG_SYN) == 0u || (syn.flags & TCP_FLAG_ACK) != 0u) {
            continue;
        }

        struct net_tcp_endpoint ep;
        ep.src_ip = local_ip;
        ep.dst_ip = syn.src_ip;
        ep.src_port = local_port;
        ep.dst_port = syn.src_port;

        int id = net_tcp_alloc(ep);
        if (id < 0) {
            return -1;
        }
        struct net_tcp_conn *c = &tcp_conns[id];
        uint32_t iss = tcp_iss_seed;
        tcp_iss_seed += 0x10001u;

        c->state = NET_TCP_SYN_RECV;
        c->send_next = iss;
        c->recv_next = syn.seq + 1u;

        if (tx_tcp_segment(c->ep.src_ip,
                           c->ep.dst_ip,
                           c->ep.src_port,
                           c->ep.dst_port,
                           c->send_next,
                           c->recv_next,
                           TCP_FLAG_SYN | TCP_FLAG_ACK,
                           NULL,
                           0) < 0) {
            c->used = 0;
            return -1;
        }
        c->send_next += 1u;

        struct net_tcp_segment ack;
        if (poll_matching_segment(c, &ack) < 0) {
            c->used = 0;
            return -1;
        }
        if ((ack.flags & TCP_FLAG_ACK) == 0u || ack.ack != c->send_next) {
            c->used = 0;
            return -1;
        }

        c->state = NET_TCP_ESTABLISHED;
        if (peer_ip_out) {
            *peer_ip_out = c->ep.dst_ip;
        }
        if (peer_port_out) {
            *peer_port_out = c->ep.dst_port;
        }
        return id;
    }

    return -1;
}

int net_tcp_send(int id, const void *buf, size_t len) {
    if (id < 0 || id >= NET_TCP_CONN_MAX || !tcp_conns[id].used || !buf) {
        return -1;
    }
    if (len == 0u) {
        return 0;
    }

    struct net_tcp_conn *c = &tcp_conns[id];
    if (c->state != NET_TCP_ESTABLISHED) {
        return -1;
    }

    size_t done = 0;
    const uint8_t *p = (const uint8_t *) buf;
    while (done < len) {
        size_t chunk = len - done;
        if (chunk > NET_TCP_PAYLOAD_MAX) {
            chunk = NET_TCP_PAYLOAD_MAX;
        }

        if (tx_tcp_segment(c->ep.src_ip,
                           c->ep.dst_ip,
                           c->ep.src_port,
                           c->ep.dst_port,
                           c->send_next,
                           c->recv_next,
                           TCP_FLAG_PSH | TCP_FLAG_ACK,
                           p + done,
                           chunk) < 0) {
            return (done > 0u) ? (int) done : -1;
        }

        c->send_next += (uint32_t) chunk;

        struct net_tcp_segment ackseg;
        if (poll_matching_segment(c, &ackseg) < 0) {
            return (done > 0u) ? (int) done : -1;
        }
        if ((ackseg.flags & TCP_FLAG_ACK) == 0u) {
            return (done > 0u) ? (int) done : -1;
        }
        if (ackseg.ack < c->send_next) {
            return (done > 0u) ? (int) done : -1;
        }
        if (ackseg.payload_len > 0u && ackseg.seq == c->recv_next) {
            size_t copy_len = ackseg.payload_len;
            if (copy_len > NET_TCP_PENDING_MAX) {
                copy_len = NET_TCP_PENDING_MAX;
            }
            memcpy(c->pending, ackseg.payload, copy_len);
            c->pending_len = copy_len;
            c->pending_off = 0;
            c->recv_next += (uint32_t) ackseg.payload_len;
            (void) tx_tcp_segment(c->ep.src_ip,
                                  c->ep.dst_ip,
                                  c->ep.src_port,
                                  c->ep.dst_port,
                                  c->send_next,
                                  c->recv_next,
                                  TCP_FLAG_ACK,
                                  NULL,
                                  0);
        }
        if ((ackseg.flags & TCP_FLAG_FIN) != 0u) {
            if (ackseg.payload_len == 0u) {
                c->recv_next = ackseg.seq + 1u;
            } else {
                c->recv_next += 1u;
            }
            (void) tx_tcp_segment(c->ep.src_ip,
                                  c->ep.dst_ip,
                                  c->ep.src_port,
                                  c->ep.dst_port,
                                  c->send_next,
                                  c->recv_next,
                                  TCP_FLAG_ACK,
                                  NULL,
                                  0);
            c->state = NET_TCP_CLOSE_WAIT;
            return (int) done;
        }

        done += chunk;
    }

    return (int) done;
}

int net_tcp_recv(int id, void *buf, size_t cap) {
    if (id < 0 || id >= NET_TCP_CONN_MAX || !tcp_conns[id].used || !buf) {
        return -1;
    }
    if (cap == 0u) {
        return 0;
    }

    struct net_tcp_conn *c = &tcp_conns[id];
    if (c->state != NET_TCP_ESTABLISHED && c->state != NET_TCP_CLOSE_WAIT) {
        return -1;
    }
    if (c->pending_off < c->pending_len) {
        size_t avail = c->pending_len - c->pending_off;
        size_t n = (avail < cap) ? avail : cap;
        memcpy(buf, c->pending + c->pending_off, n);
        c->pending_off += n;
        if (c->pending_off >= c->pending_len) {
            c->pending_off = 0;
            c->pending_len = 0;
        }
        return (int) n;
    }
    if (c->state == NET_TCP_CLOSE_WAIT) {
        return 0;
    }

    int poll_timeouts = 0;
    while (1) {
        struct net_tcp_segment seg;
        if (poll_matching_segment(c, &seg) < 0) {
            if (++poll_timeouts < 32) {
                continue;
            }
            return -1;
        }
        poll_timeouts = 0;

        if ((seg.flags & TCP_FLAG_RST) != 0u) {
            c->state = NET_TCP_CLOSED;
            return -1;
        }

        if (seg.payload_len == 0u) {
            if ((seg.flags & TCP_FLAG_FIN) != 0u) {
                c->recv_next = seg.seq + 1u;
                (void) tx_tcp_segment(c->ep.src_ip,
                                      c->ep.dst_ip,
                                      c->ep.src_port,
                                      c->ep.dst_port,
                                      c->send_next,
                                      c->recv_next,
                                      TCP_FLAG_ACK,
                                      NULL,
                                      0);
                c->state = NET_TCP_CLOSE_WAIT;
                return 0;
            }
            if ((seg.flags & TCP_FLAG_ACK) != 0u && seg.ack > c->send_next) {
                c->send_next = seg.ack;
            }
            continue;
        }

        if (seg.seq != c->recv_next) {
            (void) tx_tcp_segment(c->ep.src_ip,
                                  c->ep.dst_ip,
                                  c->ep.src_port,
                                  c->ep.dst_port,
                                  c->send_next,
                                  c->recv_next,
                                  TCP_FLAG_ACK,
                                  NULL,
                                  0);
            continue;
        }

        size_t n = (seg.payload_len < cap) ? seg.payload_len : cap;
        memcpy(buf, seg.payload, n);
        if (n < seg.payload_len) {
            size_t rem = seg.payload_len - n;
            if (rem > NET_TCP_PENDING_MAX) {
                rem = NET_TCP_PENDING_MAX;
            }
            memcpy(c->pending, seg.payload + n, rem);
            c->pending_len = rem;
            c->pending_off = 0;
        }
        c->recv_next += (uint32_t) seg.payload_len;
        if ((seg.flags & TCP_FLAG_FIN) != 0u) {
            c->recv_next += 1u;
            c->state = NET_TCP_CLOSE_WAIT;
        }

        (void) tx_tcp_segment(c->ep.src_ip,
                              c->ep.dst_ip,
                              c->ep.src_port,
                              c->ep.dst_port,
                              c->send_next,
                              c->recv_next,
                              TCP_FLAG_ACK,
                              NULL,
                              0);

        return (int) n;
    }
}

int net_tcp_close(int id) {
    if (id < 0 || id >= NET_TCP_CONN_MAX || !tcp_conns[id].used) {
        return -1;
    }

    struct net_tcp_conn *c = &tcp_conns[id];
    if (c->state == NET_TCP_ESTABLISHED || c->state == NET_TCP_CLOSE_WAIT) {
        (void) tx_tcp_segment(c->ep.src_ip,
                              c->ep.dst_ip,
                              c->ep.src_port,
                              c->ep.dst_port,
                              c->send_next,
                              c->recv_next,
                              TCP_FLAG_FIN | TCP_FLAG_ACK,
                              NULL,
                              0);
        c->send_next += 1u;
    }

    c->used = 0;
    c->state = NET_TCP_CLOSED;
    c->send_next = 0;
    c->recv_next = 0;
    c->pending_len = 0;
    c->pending_off = 0;
    return 0;
}
