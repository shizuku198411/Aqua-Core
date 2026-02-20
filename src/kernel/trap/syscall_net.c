#include "syscall_internal.h"
#include "kernel/kernel.h"
#include "net/net.h"
#include "net/udp.h"
#include "proc/process.h"
#include "kernel/page_access.h"
#include "user/socket.h"
#include "core/commonlibs.h"

#define SOCKET_MAX_PER_PROC      8
#define SOCKET_FD_BASE           64
#define UDP_PAYLOAD_MAX          1400
#define SOCKET_EPHEMERAL_MIN     49152u
#define SOCKET_EPHEMERAL_MAX     65535u
#define SOCKET_RECV_POLL_LIMIT   2000000u

struct kernel_socket {
    int used;
    uint16_t local_port; // host order
};

static struct kernel_socket socket_table[PROCS_MAX][SOCKET_MAX_PER_PROC];
static int socket_owner_pid[PROCS_MAX];
static uint16_t next_ephemeral_port = SOCKET_EPHEMERAL_MIN;

static uint16_t read_be16(const uint8_t *p) {
    return (uint16_t) (((uint16_t) p[0] << 8) | (uint16_t) p[1]);
}

static uint32_t read_be32(const uint8_t *p) {
    return ((uint32_t) p[0] << 24)
         | ((uint32_t) p[1] << 16)
         | ((uint32_t) p[2] << 8)
         | (uint32_t) p[3];
}

static uint16_t host_to_be16(uint16_t v) {
    return (uint16_t) ((v << 8) | (v >> 8));
}

static void socket_ensure_owner_row(int pid_idx, int pid) {
    if (pid_idx < 0 || pid_idx >= PROCS_MAX || pid <= 0) {
        return;
    }
    if (socket_owner_pid[pid_idx] == pid) {
        return;
    }
    memset(socket_table[pid_idx], 0, sizeof(socket_table[pid_idx]));
    socket_owner_pid[pid_idx] = pid;
}

static int socket_pid_index(void) {
    if (!current_proc) {
        return -1;
    }
    int pid_idx = process_slot_index_by_pid(current_proc->pid);
    if (pid_idx < 0) {
        return -1;
    }
    socket_ensure_owner_row(pid_idx, current_proc->pid);
    return pid_idx;
}

static int socket_idx_from_fd(int sockfd) {
    int idx = sockfd - SOCKET_FD_BASE;
    if (idx < 0 || idx >= SOCKET_MAX_PER_PROC) {
        return -1;
    }
    return idx;
}

int syscall_net_close_socket_fd(int sockfd) {
    int pid_idx = socket_pid_index();
    if (pid_idx < 0) {
        return -1;
    }
    int sidx = socket_idx_from_fd(sockfd);
    if (sidx < 0 || !socket_table[pid_idx][sidx].used) {
        return -1;
    }
    socket_table[pid_idx][sidx].used = 0;
    socket_table[pid_idx][sidx].local_port = 0;
    return 0;
}

static int alloc_socket_fd(int pid_idx, uint16_t local_port) {
    for (int i = 0; i < SOCKET_MAX_PER_PROC; i++) {
        if (!socket_table[pid_idx][i].used) {
            socket_table[pid_idx][i].used = 1;
            socket_table[pid_idx][i].local_port = local_port;
            return SOCKET_FD_BASE + i;
        }
    }
    return -1;
}

static int is_local_port_used(int pid_idx, uint16_t port) {
    for (int i = 0; i < SOCKET_MAX_PER_PROC; i++) {
        if (!socket_table[pid_idx][i].used) {
            continue;
        }
        if (socket_table[pid_idx][i].local_port == port) {
            return 1;
        }
    }
    return 0;
}

static int is_local_port_used_except(int pid_idx, uint16_t port, int except_idx) {
    for (int i = 0; i < SOCKET_MAX_PER_PROC; i++) {
        if (i == except_idx) {
            continue;
        }
        if (!socket_table[pid_idx][i].used) {
            continue;
        }
        if (socket_table[pid_idx][i].local_port == port) {
            return 1;
        }
    }
    return 0;
}

static uint16_t alloc_ephemeral_port(int pid_idx) {
    uint32_t tries = (uint32_t) (SOCKET_EPHEMERAL_MAX - SOCKET_EPHEMERAL_MIN + 1u);
    for (uint32_t i = 0; i < tries; i++) {
        uint16_t p = next_ephemeral_port;
        next_ephemeral_port++;
        if (next_ephemeral_port > SOCKET_EPHEMERAL_MAX) {
            next_ephemeral_port = SOCKET_EPHEMERAL_MIN;
        }
        if (!is_local_port_used(pid_idx, p)) {
            return p;
        }
    }
    return 0;
}

static int parse_udp_packet(const uint8_t *frame,
                            size_t frame_len,
                            uint16_t expect_dport,
                            uint32_t *src_ip_out,
                            uint16_t *src_port_out,
                            const uint8_t **payload_out,
                            size_t *payload_len_out) {
    if (!frame || !src_ip_out || !src_port_out || !payload_out || !payload_len_out) {
        return -1;
    }
    if (frame_len < 14 + 20 + 8) {
        return -1;
    }
    if (read_be16(&frame[12]) != 0x0800u) {
        return -1;
    }

    const uint8_t *ip = &frame[14];
    uint8_t version = (uint8_t) (ip[0] >> 4);
    uint8_t ihl_words = (uint8_t) (ip[0] & 0x0fu);
    size_t ihl = (size_t) ihl_words * 4u;
    if (version != 4u || ihl < 20u) {
        return -1;
    }
    if (frame_len < 14 + ihl + 8u) {
        return -1;
    }
    if (ip[9] != 17u) {
        return -1;
    }

    uint16_t ip_total = read_be16(&ip[2]);
    if (ip_total < ihl + 8u) {
        return -1;
    }
    if ((size_t) ip_total > frame_len - 14u) {
        return -1;
    }

    uint32_t dst_ip = read_be32(&ip[16]);
    if (dst_ip != net_ipv4_source_addr()) {
        return -1;
    }

    const uint8_t *udp = ip + ihl;
    uint16_t src_port = read_be16(&udp[0]);
    uint16_t dst_port = read_be16(&udp[2]);
    uint16_t udp_len = read_be16(&udp[4]);
    if (dst_port != expect_dport) {
        return -1;
    }
    if (udp_len < 8u) {
        return -1;
    }
    if ((size_t) udp_len > (size_t) ip_total - ihl) {
        return -1;
    }

    *src_ip_out = read_be32(&ip[12]);
    *src_port_out = src_port;
    *payload_out = udp + 8;
    *payload_len_out = (size_t) udp_len - 8u;
    return 0;
}

void syscall_handle_ping_tx(struct trap_frame *f) {
    uint32_t dst_ip = (uint32_t) f->a0;
    uint16_t id = (uint16_t) f->a1;
    uint16_t seq = (uint16_t) f->a2;

    int ret = net_ping_send_once(dst_ip, id, seq);
    f->a0 = ret;
}

void syscall_handle_socket(struct trap_frame *f) {
    int domain = (int) f->a0;
    int type = (int) f->a1;
    int protocol = (int) f->a2;
    int pid_idx = socket_pid_index();
    if (pid_idx < 0) {
        f->a0 = -1;
        return;
    }
    if (domain != AF_INET || type != SOCK_DGRAM) {
        f->a0 = -1;
        return;
    }
    if (!(protocol == 0 || protocol == IPPROTO_UDP)) {
        f->a0 = -1;
        return;
    }

    uint16_t port = alloc_ephemeral_port(pid_idx);
    if (port == 0) {
        f->a0 = -1;
        return;
    }

    f->a0 = alloc_socket_fd(pid_idx, port);
}

void syscall_handle_bind(struct trap_frame *f) {
    int sockfd = (int) f->a0;
    const struct socket_addr_in *user_addr = (const struct socket_addr_in *) f->a1;
    uint32_t addrlen = (uint32_t) f->a2;
    int pid_idx = socket_pid_index();
    if (pid_idx < 0) {
        f->a0 = -1;
        return;
    }
    int sidx = socket_idx_from_fd(sockfd);
    if (sidx < 0 || !socket_table[pid_idx][sidx].used) {
        f->a0 = -1;
        return;
    }
    if (!user_addr || addrlen < (uint32_t) sizeof(struct socket_addr_in)) {
        f->a0 = -1;
        return;
    }

    struct socket_addr_in addr;
    if (copyin(&addr, user_addr, sizeof(addr)) < 0) {
        f->a0 = -1;
        return;
    }
    if (addr.sin_family != AF_INET) {
        f->a0 = -1;
        return;
    }
    // For now, bind only to local interface address (or wildcard).
    if (!(addr.sin_addr == 0u || addr.sin_addr == net_ipv4_source_addr())) {
        f->a0 = -1;
        return;
    }

    uint16_t req_port = read_be16((const uint8_t *) &addr.sin_port);
    if (req_port == 0u) {
        req_port = alloc_ephemeral_port(pid_idx);
        if (req_port == 0u) {
            f->a0 = -1;
            return;
        }
    } else if (is_local_port_used_except(pid_idx, req_port, sidx)) {
        f->a0 = -1;
        return;
    }

    socket_table[pid_idx][sidx].local_port = req_port;
    f->a0 = 0;
}

void syscall_handle_sendto(struct trap_frame *f) {
    int sockfd = (int) f->a0;
    const void *user_buf = (const void *) f->a1;
    size_t len = (size_t) f->a2;
    const struct socket_addr_in *user_to = (const struct socket_addr_in *) f->a4;
    uint32_t tolen = (uint32_t) f->a5;
    int pid_idx = socket_pid_index();
    if (pid_idx < 0) {
        f->a0 = -1;
        return;
    }

    int sidx = socket_idx_from_fd(sockfd);
    if (sidx < 0 || !socket_table[pid_idx][sidx].used) {
        f->a0 = -1;
        return;
    }
    if (!user_buf && len > 0u) {
        f->a0 = -1;
        return;
    }
    if (!user_to || tolen < (uint32_t) sizeof(struct socket_addr_in)) {
        f->a0 = -1;
        return;
    }
    if (len > UDP_PAYLOAD_MAX) {
        f->a0 = -1;
        return;
    }

    struct socket_addr_in to;
    if (copyin(&to, user_to, sizeof(to)) < 0) {
        f->a0 = -1;
        return;
    }
    if (to.sin_family != AF_INET) {
        f->a0 = -1;
        return;
    }

    uint8_t kbuf[UDP_PAYLOAD_MAX];
    if (len > 0u && copyin(kbuf, user_buf, len) < 0) {
        f->a0 = -1;
        return;
    }

    uint16_t dport = read_be16((const uint8_t *) &to.sin_port);
    uint16_t sport = socket_table[pid_idx][sidx].local_port;
    int ret = net_udp_send_once(to.sin_addr, sport, dport, kbuf, len);
    f->a0 = (ret < 0) ? -1 : (int) len;
}

void syscall_handle_recvfrom(struct trap_frame *f) {
    int sockfd = (int) f->a0;
    void *user_buf = (void *) f->a1;
    size_t len = (size_t) f->a2;
    struct socket_addr_in *user_from = (struct socket_addr_in *) f->a4;
    uint32_t *user_fromlen = (uint32_t *) f->a5;
    int pid_idx = socket_pid_index();
    if (pid_idx < 0) {
        f->a0 = -1;
        return;
    }

    int sidx = socket_idx_from_fd(sockfd);
    if (sidx < 0 || !socket_table[pid_idx][sidx].used) {
        f->a0 = -1;
        return;
    }
    if (!user_buf && len > 0u) {
        f->a0 = -1;
        return;
    }

    uint16_t local_port = socket_table[pid_idx][sidx].local_port;
    for (uint32_t spin = 0; spin < SOCKET_RECV_POLL_LIMIT; spin++) {
        const uint8_t *frame = NULL;
        size_t frame_len = 0;
        int rr = net_rx_try_dequeue(&frame, &frame_len);
        if (rr < 0) {
            if (rr != -1) {
                f->a0 = -1;
                return;
            }
            continue;
        }

        uint32_t src_ip = 0;
        uint16_t src_port = 0;
        const uint8_t *payload = NULL;
        size_t payload_len = 0;
        if (parse_udp_packet(frame, frame_len, local_port, &src_ip, &src_port, &payload, &payload_len) < 0) {
            continue;
        }

        size_t copy_len = (len < payload_len) ? len : payload_len;
        if (copy_len > 0u && copyout(user_buf, payload, copy_len) < 0) {
            f->a0 = -1;
            return;
        }

        if (user_from) {
            struct socket_addr_in from;
            from.sin_family = AF_INET;
            from.sin_port = host_to_be16(src_port);
            from.sin_addr = src_ip;
            if (copyout(user_from, &from, sizeof(from)) < 0) {
                f->a0 = -1;
                return;
            }
        }
        if (user_fromlen) {
            uint32_t out_len = (uint32_t) sizeof(struct socket_addr_in);
            if (copyout(user_fromlen, &out_len, sizeof(out_len)) < 0) {
                f->a0 = -1;
                return;
            }
        }
        f->a0 = (int) copy_len;
        return;
    }

    f->a0 = -1;
}
