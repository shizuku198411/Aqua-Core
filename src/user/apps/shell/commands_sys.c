#include "commands_sys.h"
#include "commonlibs.h"
#include "user_syscall.h"
#include "user_path.h"
#include "shell.h"
#include "fs.h"

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

__attribute__((noreturn))
void shell_cmd_exit(void) {
    exit();
}
