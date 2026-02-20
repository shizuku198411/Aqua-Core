#include "commands_sys.h"
#include "core/commonlibs.h"
#include "user_syscall.h"
#include "user_path.h"
#include "shell.h"
#include "fs/fs.h"

void shell_cmd_history(void) {
    history_print();
}

void shell_cmd_stdin_test(void) {
    char buf[64];
    int total = 0;

    while (1) {
        int n = shell_read_input(buf, sizeof(buf) - 1);
        if (n <= 0) {
            break;
        }
        total += n;
        buf[n] = '\0';
        shell_printf("%s", buf);
    }
    shell_printf("\n[stdin_test] bytes=%d\n", total);
}

int shell_cmd_write_file(const char *path, const char *text) {
    if (!path || !text) {
        return -1;
    }

    int fd = fs_open(path, O_CREAT | O_WRONLY | O_TRUNC);
    if (fd < 0) {
        shell_printf("open failed\n");
        return -1;
    }

    int n = str_len(text);
    int w = fs_write(fd, text, n);
    fs_close(fd);
    if (w < 0) {
        shell_printf("write failed\n");
        return -1;
    }
    return 0;
}

int shell_cmd_cd(const char *path) {
    if (!path) {
        return -1;
    }

    char target[FS_PATH_MAX];
    if (user_path_resolve(path, target, sizeof(target)) < 0) {
        return -1;
    }

    return chdir(target);
}

int shell_cmd_pwd(void) {
    char target[FS_PATH_MAX];
    if (getcwd(target) < 0) {
        return -1;
    }
    printf("%s\n", target);
    return 0;
}

static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static int parse_hex_byte(const char *s, uint8_t *out) {
    if (!s || !out) {
        return -1;
    }

    int i = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        i = 2;
    }

    int hi = hex_nibble(s[i]);
    int lo = hex_nibble(s[i + 1]);
    if (hi < 0 || lo < 0 || s[i + 2] != '\0') {
        return -1;
    }

    *out = (uint8_t) ((hi << 4) | lo);
    return 0;
}

int shell_cmd_net_send_raw(int argc, char **argv) {
    if (!argv || argc < 2) {
        printf("usage: net_send_raw <hex-byte> [hex-byte] ...\n");
        printf("example: net_send_raw ff ff ff ff ff ff 52 54 00 12 34 56 08 00\n");
        return -1;
    }

    uint8_t frame[1600];
    int len = 0;
    for (int i = 1; i < argc; i++) {
        if (len >= (int) sizeof(frame)) {
            printf("too many bytes\n");
            return -1;
        }
        uint8_t b = 0;
        if (parse_hex_byte(argv[i], &b) < 0) {
            printf("invalid hex byte: %s\n", argv[i]);
            return -1;
        }
        frame[len++] = b;
    }

    int fd = fs_open("/dev/net0", O_WRONLY);
    if (fd < 0) {
        printf("open /dev/net0 failed\n");
        return -1;
    }

    int w = fs_write(fd, frame, len);
    fs_close(fd);
    if (w < 0) {
        printf("net_send_raw failed (%d)\n", w);
        return -1;
    }
    printf("sent %d bytes\n", w);
    return 0;
}

int shell_cmd_net_recv_raw(int max_bytes) {
    int cap = max_bytes;
    if (cap <= 0 || cap > 1600) {
        cap = 1600;
    }

    uint8_t frame[1600];
    int fd = fs_open("/dev/net0", O_RDONLY);
    if (fd < 0) {
        printf("open /dev/net0 failed\n");
        return -1;
    }

    int n = fs_read(fd, frame, cap);
    fs_close(fd);
    if (n < 0) {
        printf("net_recv_raw: no frame (%d)\n", n);
        return -1;
    }

    printf("received %d bytes\n", n);
    for (int i = 0; i < n; i++) {
        printf("%x", (unsigned) frame[i]);
        if (i + 1 < n) {
            printf(" ");
        }
        if (((i + 1) % 16) == 0) {
            printf("\n");
        }
    }
    if ((n % 16) != 0) {
        printf("\n");
    }
    return 0;
}

static int sockudp_parse_octet(const char *s, int *consumed, uint32_t *out) {
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

static int sockudp_parse_ipv4(const char *s, uint32_t *out_ip) {
    if (!s || !out_ip) {
        return -1;
    }

    uint32_t b[4] = {0, 0, 0, 0};
    int pos = 0;
    for (int i = 0; i < 4; i++) {
        int n = 0;
        if (sockudp_parse_octet(&s[pos], &n, &b[i]) < 0) {
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

static int sockudp_parse_u16(const char *s, uint16_t *out) {
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

static uint16_t sockudp_host_to_be16(uint16_t v) {
    return (uint16_t) ((v << 8) | (v >> 8));
}

static uint16_t sockudp_be16_to_host(uint16_t v) {
    return sockudp_host_to_be16(v);
}

int shell_cmd_sockudp(int argc, char **argv) {
    if (!argv || argc < 2) {
        printf("usage:\n");
        printf("  sockudp send <ipv4> <sport> <dport> <payload>\n");
        printf("  sockudp recv <port> [max_bytes]\n");
        return -1;
    }

    if (strcmp(argv[1], "send") == 0) {
        if (argc != 6) {
            printf("usage: sockudp send <ipv4> <sport> <dport> <payload>\n");
            return -1;
        }

        uint32_t dst_ip = 0;
        uint16_t sport = 0;
        uint16_t dport = 0;
        const char *payload = argv[5];
        int payload_len = str_len(payload);

        if (sockudp_parse_ipv4(argv[2], &dst_ip) < 0) {
            printf("invalid ipv4: %s\n", argv[2]);
            return -1;
        }
        if (sockudp_parse_u16(argv[3], &sport) < 0 || sockudp_parse_u16(argv[4], &dport) < 0) {
            printf("invalid port\n");
            return -1;
        }

        int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s < 0) {
            printf("socket failed\n");
            return -1;
        }

        struct socket_addr_in local;
        local.sin_family = AF_INET;
        local.sin_port = sockudp_host_to_be16(sport);
        local.sin_addr = 0;
        if (bind(s, &local, sizeof(local)) < 0) {
            printf("bind failed\n");
            fs_close(s);
            return -1;
        }

        struct socket_addr_in to;
        to.sin_family = AF_INET;
        to.sin_port = sockudp_host_to_be16(dport);
        to.sin_addr = dst_ip;

        int ret = sendto(s, payload, payload_len, &to, sizeof(to));
        fs_close(s);
        if (ret < 0) {
            printf("sockudp send failed (%d)\n", ret);
            return -1;
        }

        printf("sockudp sent %d bytes to %s:%d (sport=%d)\n", ret, argv[2], (int) dport, (int) sport);
        return 0;
    }

    if (strcmp(argv[1], "recv") == 0) {
        if (argc != 3 && argc != 4) {
            printf("usage: sockudp recv <port> [max_bytes]\n");
            return -1;
        }

        uint16_t port = 0;
        if (sockudp_parse_u16(argv[2], &port) < 0) {
            printf("invalid port\n");
            return -1;
        }

        int cap = 256;
        if (argc == 4) {
            if (parse_int(argv[3], &cap) < 0 || cap <= 0 || cap > 1400) {
                printf("invalid max_bytes (1..1400)\n");
                return -1;
            }
        }

        uint8_t buf[1400];
        int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s < 0) {
            printf("socket failed\n");
            return -1;
        }

        struct socket_addr_in local;
        local.sin_family = AF_INET;
        local.sin_port = sockudp_host_to_be16(port);
        local.sin_addr = 0;
        if (bind(s, &local, sizeof(local)) < 0) {
            printf("bind failed\n");
            fs_close(s);
            return -1;
        }

        struct socket_addr_in from;
        uint32_t fromlen = sizeof(from);
        int n = recvfrom(s, buf, cap, &from, &fromlen);
        fs_close(s);
        if (n < 0) {
            printf("sockudp recv failed (%d)\n", n);
            return -1;
        }

        uint32_t ip = from.sin_addr;
        uint16_t src_port = sockudp_be16_to_host(from.sin_port);
        printf("sockudp recv %d bytes from %d.%d.%d.%d:%d\n",
               n,
               (int) ((ip >> 24) & 0xffu),
               (int) ((ip >> 16) & 0xffu),
               (int) ((ip >> 8) & 0xffu),
               (int) (ip & 0xffu),
               (int) src_port);

        for (int i = 0; i < n; i++) {
            char c = (char) buf[i];
            if (c < 0x20 || c > 0x7e) {
                c = '.';
            }
            putchar(c);
        }
        putchar('\n');
        return 0;
    }

    printf("usage:\n");
    printf("  sockudp send <ipv4> <sport> <dport> <payload>\n");
    printf("  sockudp recv <port> [max_bytes]\n");
    return -1;
}

__attribute__((noreturn))
void shell_cmd_exit(void) {
    exit(0);
}
