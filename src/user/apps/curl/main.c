#include "core/commonlibs.h"
#include "user_syscall.h"

#define CURL_HOST_MAX        128
#define CURL_PATH_MAX        256
#define CURL_REQ_MAX         512
#define CURL_RX_MAX          1024
#define CURL_DNS_QUERY_CAP   512
#define CURL_DNS_RECV_CAP    1024
#define DNS_DEFAULT_IP       0x08080808u
#define DNS_PORT             53u
#define HTTP_PORT            80u
#define DNS_RETRY_MAX        400u
#define DNS_RETRY_SLEEP_MS   10u

static int local_strlen(const char *s) {
    int n = 0;
    while (s && s[n] != '\0') {
        n++;
    }
    return n;
}

static int starts_with(const char *s, const char *prefix) {
    int i = 0;
    while (prefix[i] != '\0') {
        if (s[i] != prefix[i]) {
            return 0;
        }
        i++;
    }
    return 1;
}

static int parse_octet(const char *s, int *consumed, uint32_t *out) {
    if (!s || !consumed || !out) return -1;
    if (s[0] < '0' || s[0] > '9') return -1;
    uint32_t v = 0;
    int i = 0;
    while (s[i] >= '0' && s[i] <= '9') {
        v = v * 10u + (uint32_t) (s[i] - '0');
        if (v > 255u) return -1;
        i++;
    }
    *consumed = i;
    *out = v;
    return 0;
}

static int parse_ipv4(const char *s, uint32_t *out_ip) {
    if (!s || !out_ip) return -1;
    uint32_t b[4] = {0, 0, 0, 0};
    int pos = 0;
    for (int i = 0; i < 4; i++) {
        int n = 0;
        if (parse_octet(&s[pos], &n, &b[i]) < 0) return -1;
        pos += n;
        if (i < 3) {
            if (s[pos] != '.') return -1;
            pos++;
        }
    }
    if (s[pos] != '\0') return -1;
    *out_ip = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
    return 0;
}

static int parse_u16(const char *s, uint16_t *out) {
    if (!s || !out || s[0] == '\0') return -1;
    uint32_t v = 0;
    for (int i = 0; s[i] != '\0'; i++) {
        if (s[i] < '0' || s[i] > '9') return -1;
        v = v * 10u + (uint32_t) (s[i] - '0');
        if (v > 65535u) return -1;
    }
    *out = (uint16_t) v;
    return 0;
}

static uint16_t host_to_be16(uint16_t v) {
    return (uint16_t) ((v << 8) | (v >> 8));
}

static uint16_t read_be16(const uint8_t *p) {
    return (uint16_t) (((uint16_t) p[0] << 8) | (uint16_t) p[1]);
}

static void write_be16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t) ((v >> 8) & 0xffu);
    p[1] = (uint8_t) (v & 0xffu);
}

static int parse_url(const char *url, char host[CURL_HOST_MAX], char path[CURL_PATH_MAX], uint16_t *port_out) {
    if (!url || !host || !path || !port_out) {
        return -1;
    }

    const char *p = url;
    if (starts_with(p, "http://")) {
        p += 7;
    }

    int hi = 0;
    int pi = 0;
    uint16_t port = HTTP_PORT;

    while (*p && *p != '/' && *p != ':') {
        if (hi >= CURL_HOST_MAX - 1) {
            return -1;
        }
        host[hi++] = *p++;
    }
    host[hi] = '\0';
    if (hi == 0) {
        return -1;
    }

    if (*p == ':') {
        p++;
        char portbuf[8];
        int n = 0;
        while (*p && *p != '/') {
            if (n >= (int) sizeof(portbuf) - 1) {
                return -1;
            }
            portbuf[n++] = *p++;
        }
        portbuf[n] = '\0';
        if (parse_u16(portbuf, &port) < 0 || port == 0u) {
            return -1;
        }
    }

    if (*p == '\0') {
        path[0] = '/';
        path[1] = '\0';
    } else {
        while (*p) {
            if (pi >= CURL_PATH_MAX - 1) {
                return -1;
            }
            path[pi++] = *p++;
        }
        path[pi] = '\0';
    }

    *port_out = port;
    return 0;
}

static int encode_dns_qname(const char *name, uint8_t *out, int cap) {
    if (!name || !out || cap <= 0) return -1;

    int n = 0;
    int label_start = 0;
    int i = 0;
    while (1) {
        char c = name[i];
        int at_end = (c == '\0');
        int at_dot = (c == '.');
        if (at_dot || at_end) {
            int label_len = i - label_start;
            if (label_len < 0 || label_len > 63) return -1;
            if (n + 1 + label_len >= cap) return -1;
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

    if (n >= cap) return -1;
    out[n++] = 0;
    return n;
}

static int build_dns_query(const char *name, uint16_t txid, uint8_t *out, int cap) {
    if (!name || !out || cap < 12) return -1;
    memset(out, 0, 12);

    write_be16(&out[0], txid);
    write_be16(&out[2], 0x0100u);
    write_be16(&out[4], 1u);

    int off = 12;
    int qname_len = encode_dns_qname(name, &out[off], cap - off);
    if (qname_len < 0) return -1;
    off += qname_len;
    if (off + 4 > cap) return -1;

    write_be16(&out[off], 1u);
    write_be16(&out[off + 2], 1u);
    return off + 4;
}

static int dns_skip_name(const uint8_t *msg, int len, int off) {
    int limit = 0;
    while (off < len && limit++ < 128) {
        uint8_t c = msg[off];
        if (c == 0) return off + 1;
        if ((c & 0xc0u) == 0xc0u) {
            if (off + 1 >= len) return -1;
            return off + 2;
        }
        if (c > 63u) return -1;
        off += 1 + (int) c;
    }
    return -1;
}

static int parse_dns_first_a(const uint8_t *dns, int dns_len, uint32_t *ip_out) {
    if (!dns || !ip_out || dns_len < 12) return -1;

    uint16_t qdcount = read_be16(&dns[4]);
    uint16_t ancount = read_be16(&dns[6]);
    int off = 12;

    for (uint16_t i = 0; i < qdcount; i++) {
        off = dns_skip_name(dns, dns_len, off);
        if (off < 0 || off + 4 > dns_len) return -1;
        off += 4;
    }

    for (uint16_t i = 0; i < ancount; i++) {
        off = dns_skip_name(dns, dns_len, off);
        if (off < 0 || off + 10 > dns_len) return -1;

        uint16_t typ = read_be16(&dns[off]);
        uint16_t cls = read_be16(&dns[off + 2]);
        uint16_t rdlen = read_be16(&dns[off + 8]);
        off += 10;
        if (off + rdlen > dns_len) return -1;
        if (typ == 1u && cls == 1u && rdlen == 4u) {
            *ip_out = ((uint32_t) dns[off] << 24)
                    | ((uint32_t) dns[off + 1] << 16)
                    | ((uint32_t) dns[off + 2] << 8)
                    | (uint32_t) dns[off + 3];
            return 0;
        }
        off += rdlen;
    }

    return -1;
}

static int resolve_host_ipv4(const char *host, uint32_t *ip_out) {
    if (!host || !ip_out) return -1;

    if (parse_ipv4(host, ip_out) == 0) {
        return 0;
    }

    uint8_t qbuf[CURL_DNS_QUERY_CAP];
    int qlen = build_dns_query(host, 0x4242u, qbuf, sizeof(qbuf));
    if (qlen < 0) {
        return -1;
    }

    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) return -1;

    struct socket_addr_in local;
    local.sin_family = AF_INET;
    local.sin_port = host_to_be16(53000u);
    local.sin_addr = 0;
    if (bind(s, &local, sizeof(local)) < 0) {
        fs_close(s);
        return -1;
    }

    struct socket_addr_in dns;
    dns.sin_family = AF_INET;
    dns.sin_port = host_to_be16(DNS_PORT);
    dns.sin_addr = DNS_DEFAULT_IP;

    if (sendto(s, qbuf, qlen, &dns, sizeof(dns)) < 0) {
        fs_close(s);
        return -1;
    }

    uint8_t rbuf[CURL_DNS_RECV_CAP];
    for (uint32_t i = 0; i < DNS_RETRY_MAX; i++) {
        struct socket_addr_in from;
        uint32_t fromlen = sizeof(from);
        int n = recvfrom(s, rbuf, sizeof(rbuf), &from, &fromlen);
        if (n < 0) {
            sleep(DNS_RETRY_SLEEP_MS);
            continue;
        }
        if (from.sin_addr != DNS_DEFAULT_IP) {
            continue;
        }
        if (parse_dns_first_a(rbuf, n, ip_out) == 0) {
            fs_close(s);
            return 0;
        }
    }

    fs_close(s);
    return -1;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("usage: curl <http://host[:port]/path>\n");
        return -1;
    }

    char host[CURL_HOST_MAX];
    char path[CURL_PATH_MAX];
    uint16_t port = HTTP_PORT;
    if (parse_url(argv[1], host, path, &port) < 0) {
        printf("invalid url\n");
        return -1;
    }

    uint32_t dst_ip = 0;
    if (resolve_host_ipv4(host, &dst_ip) < 0) {
        printf("dns resolve failed\n");
        return -1;
    }

    int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) {
        printf("socket failed\n");
        return -1;
    }

    struct socket_addr_in to;
    to.sin_family = AF_INET;
    to.sin_port = host_to_be16(port);
    to.sin_addr = dst_ip;
    if (connect(s, &to, sizeof(to)) < 0) {
        printf("connect failed\n");
        fs_close(s);
        return -1;
    }

    char req[CURL_REQ_MAX];
    req[0] = '\0';
    strcat_s(req, sizeof(req), "GET ");
    strcat_s(req, sizeof(req), path);
    strcat_s(req, sizeof(req), " HTTP/1.0\r\nHost: ");
    strcat_s(req, sizeof(req), host);
    strcat_s(req, sizeof(req), "\r\nUser-Agent: aqua-curl/0.1\r\nConnection: close\r\n\r\n");

    int req_len = local_strlen(req);
    int sent = 0;
    while (sent < req_len) {
        int n = send(s, req + sent, req_len - sent);
        if (n <= 0) {
            printf("send failed\n");
            fs_close(s);
            return -1;
        }
        sent += n;
    }

    char rx[CURL_RX_MAX];
    int received_any = 0;
    while (1) {
        int n = recv(s, rx, sizeof(rx));
        if (n < 0) {
            if (received_any) {
                break;
            }
            printf("recv failed\n");
            fs_close(s);
            return -1;
        }
        if (n == 0) {
            break;
        }
        received_any = 1;
        for (int i = 0; i < n; i++) {
            putchar(rx[i]);
        }
    }

    fs_close(s);
    return 0;
}
