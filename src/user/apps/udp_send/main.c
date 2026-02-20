#include "core/commonlibs.h"
#include "user_syscall.h"

#define UDP_SEND_PAYLOAD_MAX 512

static int local_strlen(const char *s) {
    int n = 0;
    while (s && s[n] != '\0') {
        n++;
    }
    return n;
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

static int parse_u16(const char *s, uint16_t *out) {
    if (!s || !out || *s == '\0') {
        return -1;
    }
    uint32_t v = 0;
    for (int i = 0; s[i] != '\0'; i++) {
        if (s[i] < '0' || s[i] > '9') {
            return -1;
        }
        v = v * 10u + (uint32_t) (s[i] - '0');
        if (v > 65535u) {
            return -1;
        }
    }
    *out = (uint16_t) v;
    return 0;
}

static uint16_t host_to_be16(uint16_t v) {
    return (uint16_t) ((v << 8) | (v >> 8));
}

int main(int argc, char **argv) {
    if (argc != 5) {
        printf("usage: udp_send <ipv4> <sport> <dport> <payload>\n");
        return -1;
    }

    uint32_t dst_ip = 0;
    uint16_t sport = 0;
    uint16_t dport = 0;
    const char *payload = argv[4];
    int payload_len = local_strlen(payload);

    if (parse_ipv4(argv[1], &dst_ip) < 0) {
        printf("invalid ipv4: %s\n", argv[1]);
        return -1;
    }
    if (parse_u16(argv[2], &sport) < 0 || parse_u16(argv[3], &dport) < 0) {
        printf("invalid port\n");
        return -1;
    }

    if (payload_len > UDP_SEND_PAYLOAD_MAX) {
        printf("payload too large (max %d)\n", UDP_SEND_PAYLOAD_MAX);
        return -1;
    }

    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) {
        printf("socket failed\n");
        return -1;
    }

    struct socket_addr_in local;
    local.sin_family = AF_INET;
    local.sin_port = host_to_be16(sport);
    local.sin_addr = 0; // wildcard (local interface)
    if (bind(s, &local, sizeof(local)) < 0) {
        printf("bind failed\n");
        fs_close(s);
        return -1;
    }

    struct socket_addr_in to;
    to.sin_family = AF_INET;
    to.sin_port = host_to_be16(dport);
    to.sin_addr = dst_ip;

    int ret = sendto(s, payload, payload_len, &to, sizeof(to));
    fs_close(s);
    if (ret < 0) {
        printf("udp_send failed (%d)\n", ret);
        return -1;
    }

    printf("udp_send: dst=%s sport=%d dport=%d len=%d\n",
           argv[1], (int) sport, (int) dport, payload_len);
    return 0;
}
