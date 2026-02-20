#include "core/commonlibs.h"
#include "user_syscall.h"

#define DNS_QUERY_CAP       512
#define DNS_RX_CAP          1024
#define DNS_DEFAULT_IP      0x08080808u  // 8.8.8.8
#define DNS_DST_PORT        53u
#define DNS_SRC_PORT        53000u
#define DNS_RETRY_MAX       500u
#define DNS_RETRY_SLEEP_MS  10u

static uint16_t read_be16(const uint8_t *p) {
    return (uint16_t) (((uint16_t) p[0] << 8) | (uint16_t) p[1]);
}

static void write_be16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t) ((v >> 8) & 0xffu);
    p[1] = (uint8_t) (v & 0xffu);
}

static uint16_t host_to_be16(uint16_t v) {
    return (uint16_t) ((v << 8) | (v >> 8));
}

static uint16_t be16_to_host(uint16_t v) {
    return host_to_be16(v);
}

static int parse_octet(const char *s, int *consumed, uint32_t *out) {
    if (!s || !consumed || !out) {
        return -1;
    }
    if (s[0] < '0' || s[0] > '9') {
        return -1;
    }

    uint32_t v = 0;
    int i = 0;
    while (s[i] >= '0' && s[i] <= '9') {
        v = v * 10u + (uint32_t) (s[i] - '0');
        if (v > 255u) {
            return -1;
        }
        i++;
    }
    *consumed = i;
    *out = v;
    return 0;
}

static int parse_ipv4(const char *s, uint32_t *out_ip) {
    if (!s || !out_ip) {
        return -1;
    }

    uint32_t b[4] = {0, 0, 0, 0};
    int pos = 0;
    for (int i = 0; i < 4; i++) {
        int n = 0;
        if (parse_octet(&s[pos], &n, &b[i]) < 0) {
            return -1;
        }
        pos += n;
        if (i < 3) {
            if (s[pos] != '.') {
                return -1;
            }
            pos++;
        }
    }
    if (s[pos] != '\0') {
        return -1;
    }

    *out_ip = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
    return 0;
}

static int encode_dns_qname(const char *name, uint8_t *out, int cap) {
    if (!name || !out || cap <= 0) {
        return -1;
    }

    int n = 0;
    int label_start = 0;
    int i = 0;
    while (1) {
        char c = name[i];
        int at_end = (c == '\0');
        int at_dot = (c == '.');
        if (at_dot || at_end) {
            int label_len = i - label_start;
            if (label_len < 0 || label_len > 63) {
                return -1;
            }
            if (n + 1 + label_len >= cap) {
                return -1;
            }
            out[n++] = (uint8_t) label_len;
            for (int j = 0; j < label_len; j++) {
                out[n++] = (uint8_t) name[label_start + j];
            }
            label_start = i + 1;
            if (at_end) {
                break;
            }
        }
        i++;
    }

    if (n >= cap) {
        return -1;
    }
    out[n++] = 0;  // root label
    return n;
}

static int build_dns_query(const char *name, uint16_t txid, uint8_t *out, int cap) {
    if (!name || !out || cap < 12) {
        return -1;
    }

    for (int i = 0; i < 12; i++) {
        out[i] = 0;
    }
    write_be16(&out[0], txid);
    write_be16(&out[2], 0x0100u); // RD=1
    write_be16(&out[4], 1u);      // QDCOUNT=1

    int off = 12;
    int qname_len = encode_dns_qname(name, &out[off], cap - off);
    if (qname_len < 0) {
        return -1;
    }
    off += qname_len;
    if (off + 4 > cap) {
        return -1;
    }
    write_be16(&out[off], 1u);      // QTYPE=A
    write_be16(&out[off + 2], 1u);  // QCLASS=IN
    off += 4;
    return off;
}

static int dns_skip_name(const uint8_t *msg, int len, int off) {
    int limit = 0;
    while (off < len && limit++ < 128) {
        uint8_t c = msg[off];
        if (c == 0) {
            return off + 1;
        }
        if ((c & 0xc0u) == 0xc0u) {
            if (off + 1 >= len) {
                return -1;
            }
            return off + 2;
        }
        if (c > 63u) {
            return -1;
        }
        off += 1 + (int) c;
    }
    return -1;
}

static int parse_dns_answer_a(const uint8_t *dns, int dns_len) {
    if (!dns || dns_len < 12) {
        return -1;
    }
    uint16_t qdcount = read_be16(&dns[4]);
    uint16_t ancount = read_be16(&dns[6]);
    int off = 12;

    for (uint16_t i = 0; i < qdcount; i++) {
        off = dns_skip_name(dns, dns_len, off);
        if (off < 0 || off + 4 > dns_len) {
            return -1;
        }
        off += 4;
    }

    int found = 0;
    for (uint16_t i = 0; i < ancount; i++) {
        off = dns_skip_name(dns, dns_len, off);
        if (off < 0 || off + 10 > dns_len) {
            return -1;
        }
        uint16_t typ = read_be16(&dns[off]);
        uint16_t cls = read_be16(&dns[off + 2]);
        uint16_t rdlen = read_be16(&dns[off + 8]);
        off += 10;
        if (off + rdlen > dns_len) {
            return -1;
        }
        if (typ == 1u && cls == 1u && rdlen == 4u) {
            printf("%d.%d.%d.%d\n",
                   (int) dns[off],
                   (int) dns[off + 1],
                   (int) dns[off + 2],
                   (int) dns[off + 3]);
            found++;
        }
        off += rdlen;
    }

    return found;
}

int main(int argc, char **argv) {
    if (argc != 2 && argc != 3) {
        printf("usage: nslookup <name> [dns-server-ipv4]\n");
        return -1;
    }

    const char *name = argv[1];
    uint32_t dns_ip = DNS_DEFAULT_IP;
    if (argc == 3 && parse_ipv4(argv[2], &dns_ip) < 0) {
        printf("invalid ipv4: %s\n", argv[2]);
        return -1;
    }

    uint8_t dns_query[DNS_QUERY_CAP];
    uint16_t txid = 0x1234u;
    int dns_query_len = build_dns_query(name, txid, dns_query, sizeof(dns_query));
    if (dns_query_len < 0) {
        printf("build dns query failed\n");
        return -1;
    }

    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) {
        printf("socket failed\n");
        return -1;
    }

    struct socket_addr_in local;
    local.sin_family = AF_INET;
    local.sin_port = host_to_be16(DNS_SRC_PORT);
    local.sin_addr = 0; // wildcard (local interface)
    if (bind(s, &local, sizeof(local)) < 0) {
        printf("bind failed\n");
        fs_close(s);
        return -1;
    }

    struct socket_addr_in to;
    to.sin_family = AF_INET;
    to.sin_port = host_to_be16(DNS_DST_PORT);
    to.sin_addr = dns_ip;

    int tx = sendto(s, dns_query, dns_query_len, &to, sizeof(to));
    if (tx < 0) {
        printf("dns query send failed (%d)\n", tx);
        fs_close(s);
        return -1;
    }

    uint8_t dns_rx[DNS_RX_CAP];
    for (uint32_t i = 0; i < DNS_RETRY_MAX; i++) {
        struct socket_addr_in from;
        uint32_t fromlen = sizeof(from);
        int n = recvfrom(s, dns_rx, sizeof(dns_rx), &from, &fromlen);
        if (n < 0) {
            sleep(DNS_RETRY_SLEEP_MS);
            continue;
        }

        if (from.sin_family != AF_INET) {
            continue;
        }
        if (from.sin_addr != dns_ip) {
            continue;
        }
        if (be16_to_host(from.sin_port) != DNS_DST_PORT) {
            continue;
        }
        if (n < 12) {
            continue;
        }

        uint16_t r_txid = read_be16(&dns_rx[0]);
        uint16_t flags = read_be16(&dns_rx[2]);
        if (r_txid != txid) {
            continue;
        }
        if ((flags & 0x8000u) == 0) { // QR must be response
            continue;
        }

        int a_count = parse_dns_answer_a(dns_rx, n);
        if (a_count > 0) {
            fs_close(s);
            return 0;
        }

        printf("no A record found\n");
        fs_close(s);
        return -1;
    }

    printf("dns query timeout\n");
    fs_close(s);
    return -1;
}
