#include "core/commonlibs.h"
#include "user_syscall.h"
#include "fs/fs.h"

#define NET0_UDP_MAGIC 0x55445030u  // "UDP0"
#define UDP_SEND_REQ_MAX 512

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

    int req_len = 12 + payload_len;
    if (req_len > UDP_SEND_REQ_MAX) {
        printf("payload too large (max %d)\n", UDP_SEND_REQ_MAX - 12);
        return -1;
    }

    uint8_t req[UDP_SEND_REQ_MAX];
    req[0] = (uint8_t) ((NET0_UDP_MAGIC >> 24) & 0xffu);
    req[1] = (uint8_t) ((NET0_UDP_MAGIC >> 16) & 0xffu);
    req[2] = (uint8_t) ((NET0_UDP_MAGIC >> 8) & 0xffu);
    req[3] = (uint8_t) (NET0_UDP_MAGIC & 0xffu);
    req[4] = (uint8_t) ((dst_ip >> 24) & 0xffu);
    req[5] = (uint8_t) ((dst_ip >> 16) & 0xffu);
    req[6] = (uint8_t) ((dst_ip >> 8) & 0xffu);
    req[7] = (uint8_t) (dst_ip & 0xffu);
    req[8] = (uint8_t) ((sport >> 8) & 0xffu);
    req[9] = (uint8_t) (sport & 0xffu);
    req[10] = (uint8_t) ((dport >> 8) & 0xffu);
    req[11] = (uint8_t) (dport & 0xffu);
    if (payload_len > 0) {
        memcpy(&req[12], payload, (size_t) payload_len);
    }

    int netfd = fs_open("/dev/net0", O_WRONLY);
    if (netfd < 0) {
        printf("open /dev/net0 failed\n");
        return -1;
    }

    int ret = fs_write(netfd, req, req_len);
    fs_close(netfd);
    if (ret < 0) {
        printf("udp_send failed (%d)\n", ret);
        return -1;
    }

    printf("udp_send: dst=%s sport=%d dport=%d len=%d\n",
           argv[1], (int) sport, (int) dport, payload_len);
    return 0;
}
