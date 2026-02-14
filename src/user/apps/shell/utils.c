#include "shell.h"
#include "syscall.h"
#include "commonlibs.h"
#include "process.h"
#include "user_syscall.h"
#include "user_apps.h"
#include "fs.h"

static int shell_output_fd = -1;
static int shell_input_fd = -1;

void shell_set_input_fd(int fd) {
    shell_input_fd = fd;
}

void shell_reset_input_fd(void) {
    shell_input_fd = -1;
}

int shell_read_input(void *buf, size_t size) {
    if (!buf || size == 0) {
        return -1;
    }

    if (shell_input_fd >= 0) {
        return fs_read(shell_input_fd, buf, size);
    }

    char *cbuf = (char *) buf;
    long ch = getchar();
    if (ch < 0) {
        return -1;
    }
    cbuf[0] = (char) ch;
    return 1;
}

void shell_set_output_fd(int fd) {
    shell_output_fd = fd;
}

void shell_reset_output_fd(void) {
    shell_output_fd = -1;
}

static void shell_putc(char ch) {
    if (shell_output_fd >= 0) {
        (void) fs_write(shell_output_fd, &ch, 1);
    } else {
        putchar(ch);
    }
}

void shell_printf(const char *fmt, ...) {
    va_list vargs;
    va_start(vargs, fmt);

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case '\0':
                    shell_putc('%');
                    goto end;
                case '%':
                    shell_putc('%');
                    break;
                case 's': {
                    const char *s = va_arg(vargs, const char *);
                    while (*s) {
                        shell_putc(*s);
                        s++;
                    }
                    break;
                }
                case 'd': {
                    int value = va_arg(vargs, int);
                    unsigned magnitude = value;
                    if (value < 0) {
                        shell_putc('-');
                        magnitude = -magnitude;
                    }
                    unsigned divisor = 1;
                    while (magnitude / divisor > 9) {
                        divisor *= 10;
                    }
                    while (divisor > 0) {
                        shell_putc('0' + magnitude / divisor);
                        magnitude %= divisor;
                        divisor /= 10;
                    }
                    break;
                }
                case 'x': {
                    unsigned value = va_arg(vargs, unsigned);
                    for (int i = 7; i >= 0; i--) {
                        unsigned nibble = (value >> (i * 4)) & 0xf;
                        shell_putc("0123456789abcdef"[nibble]);
                    }
                    break;
                }
                default:
                    shell_putc('%');
                    shell_putc(*fmt);
                    break;
            }
        } else {
            shell_putc(*fmt);
        }
        fmt++;
    }

end:
    va_end(vargs);
}


int split_args(char *line, char **argv, int argv_max) {
    int argc = 0;
    int in_token = 0;

    for (int i = 0; line[i] != '\0'; i++) {
        if (line[i] == ' ' || line[i] == '\t') {
            line[i] = '\0';
            in_token = 0;
            continue;
        }

        if (!in_token) {
            if (argc < argv_max) {
                argv[argc++] = &line[i];
            }
            in_token = 1;
        }
    }

    return argc;
}

int parse_int(const char *s, int *out) {
    int value = 0;
    if (!s || *s == '\0') {
        return -1;
    }

    while (*s) {
        if (*s < '0' || *s > '9') {
            return -1;
        }
        value = value * 10 + (*s - '0');
        s++;
    }

    *out = value;
    return 0;
}

int parse_redirection(char **argv, int argc, char **exec_argv, int *exec_argc, char **in_path, char **out_path) {
    if (!argv || argc < 0) {
        return -1;
    }
    if (!in_path || !out_path) {
        return -1;
    }

    *in_path = NULL;
    *out_path = NULL;

    int outc = 0;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "<") == 0 || strcmp(argv[i], ">") == 0) {
            int is_input = (argv[i][0] == '<');
            if (i + 1 >= argc) {
                return -1;
            }

            if (is_input) {
                *in_path = argv[i + 1];
            } else {
                *out_path = argv[i + 1];
            }

            i++; // skip path token
            continue;
        }

        if (exec_argv && exec_argc) {
            exec_argv[outc++] = argv[i];
        }
    }

    if (exec_argc) {
        *exec_argc = outc;
    }
    return 0;
}

void redraw_cmdline(const char *line, int cursor_pos) {
    // Move to line head, redraw prompt, clear rest of line, then print cmdline.
    printf("\r\033[34maqua-core\033[0m:$ \033[K%s", line);

    int len = str_len(line);
    if (cursor_pos < 0) {
        cursor_pos = 0;
    }
    if (cursor_pos > len) {
        cursor_pos = len;
    }

    for (int i = cursor_pos; i < len; i++) {
        putchar('\b');
    }
}

int str_len(const char *s) {
    int n = 0;
    while (s && s[n] != '\0') {
        n++;
    }
    return n;
}

// history ling buffer
static char history[HISTORY_MAX][CMDLINE_MAX];
static char history_path[10] = "/.history";
static int history_head = 0;
static int history_count = 0;

void history_push(const char *line) {
    if (!line || line[0] == '\0') {
        return;
    }

    int write_idx;
    if (history_count < HISTORY_MAX) {
        write_idx = (history_head + history_count) % HISTORY_MAX;
        history_count++;
    } else {
        write_idx = history_head;
        history_head = (history_head + 1) % HISTORY_MAX;
    }

    int i = 0;
    for (; i < CMDLINE_MAX - 1 && line[i] != '\0'; i++) {
        history[write_idx][i] = line[i];
    }
    history[write_idx][i] = '\0';
}

void history_print(void) {
    for (int i = 0; i < history_count; i++) {
        int idx = (history_head + i) % HISTORY_MAX;
        shell_printf("%d: %s\n", i + 1, history[idx]);
    }
}

int history_size(void) {
    return history_count;
}

int history_get(int pos, char *out, int out_max) {
    if (!out || out_max <= 0) {
        return -1;
    }
    if (pos < 0 || pos >= history_count) {
        return -1;
    }

    int idx = (history_head + pos) % HISTORY_MAX;
    int i = 0;
    for (; i < out_max - 1 && history[idx][i] != '\0'; i++) {
        out[i] = history[idx][i];
    }
    out[i] = '\0';
    return 0;
}

static size_t history_encoded_line_len(const char *s) {
    size_t n = 1; // trailing '\n'
    while (*s) {
        char c = *s++;
        n += (c == '\n' || c == '\\') ? 2 : 1;
    }
    return n;
}

static size_t history_serialize(char *out, size_t out_cap) {
    if (!out || out_cap == 0) {
        return 0;
    }

    // Keep most recent entries within persistent file capacity.
    int start = history_count;
    size_t total = 0;
    for (int i = history_count - 1; i >= 0; i--) {
        int idx = (history_head + i) % HISTORY_MAX;
        size_t need = history_encoded_line_len(history[idx]);
        if (total + need > out_cap) {
            break;
        }
        total += need;
        start = i;
    }

    size_t pos = 0;
    for (int i = start; i < history_count; i++) {
        int idx = (history_head + i) % HISTORY_MAX;
        const char *s = history[idx];
        while (*s && pos < out_cap) {
            char c = *s++;
            if (c == '\n' || c == '\\') {
                if (pos + 2 > out_cap) {
                    return pos;
                }
                out[pos++] = '\\';
                out[pos++] = (c == '\n') ? 'n' : '\\';
            } else {
                if (pos + 1 > out_cap) {
                    return pos;
                }
                out[pos++] = c;
            }
        }
        if (pos + 1 > out_cap) {
            return pos;
        }
        out[pos++] = '\n';
    }
    return pos;
}

void history_load(void) {
    int fd = fs_open(history_path, O_RDONLY);
    if (fd < 0) {
        return;
    }

    history_head = 0;
    history_count = 0;

    char line[CMDLINE_MAX];
    int line_len = 0;
    int escaped = 0;

    char buf[256];
    while (1) {
        int n = fs_read(fd, buf, sizeof(buf));
        if (n <= 0) {
            break;
        }

        for (int i = 0; i < n; i++) {
            char c = buf[i];

            if (c == '\r') {
                continue;
            }

            if (escaped) {
                if (c == 'n') {
                    c = '\n';
                }
                escaped = 0;
            } else if (c == '\\') {
                escaped = 1;
                continue;
            } else if (c == '\n') {
                line[line_len] = '\0';
                history_push(line);
                line_len = 0;
                continue;
            }

            if (line_len < CMDLINE_MAX - 1) {
                line[line_len++] = c;
            }
        }
    }

    if (escaped && line_len < CMDLINE_MAX - 1) {
        line[line_len++] = '\\';
    }
    if (line_len > 0) {
        line[line_len] = '\0';
        history_push(line);
    }

    fs_close(fd);
}

void history_write(void) {
    int fd = fs_open(history_path, O_CREAT | O_WRONLY | O_TRUNC);
    if (fd < 0) {
        printf("open failed\n");
        return;
    }

    char buf[FS_FILE_MAX_SIZE];
    size_t n = history_serialize(buf, sizeof(buf));
    int w = fs_write(fd, buf, (int) n);
    if (w < 0 || (size_t) w != n) {
        printf("write failed\n");
    }
    fs_close(fd);
}

bool is_background(char **argv, int argc) {
    if (strcmp(argv[argc-1], "&") == 0) {
        return true;
    }
    return false;
}

static int app_map(const char *name) {
    if (strcmp(name, APP_NAME_SHELL) == 0) {
        return APP_ID_SHELL;
    }
    else if (strcmp(name, APP_NAME_IPC_RX) == 0) {
        return APP_ID_IPC_RX;
    }
    else if (strcmp(name, APP_NAME_PS) == 0) {
        return APP_ID_PS;
    }
    else if (strcmp(name, APP_NAME_DATE) == 0) {
        return APP_ID_DATE;
    }
    else if (strcmp(name, APP_NAME_LS) == 0) {
        return APP_ID_LS;
    }
    else if (strcmp(name, APP_NAME_MKDIR) == 0) {
        return APP_ID_MKDIR;
    }
    else if (strcmp(name, APP_NAME_RMDIR) == 0) {
        return APP_ID_RMDIR;
    }
    else if (strcmp(name, APP_NAME_TOUCH) == 0) {
        return APP_ID_TOUCH;
    }
    else if (strcmp(name, APP_NAME_RM) == 0) {
        return APP_ID_RM;
    }
    else if (strcmp(name, APP_NAME_WRITE) == 0) {
        return APP_ID_WRITE;
    }
    else if (strcmp(name, APP_NAME_CAT) == 0) {
        return APP_ID_CAT;
    }
    else if (strcmp(name, APP_NAME_KILL) == 0) {
        return APP_ID_KILL;
    }
    else if (strcmp(name, APP_NAME_KERNEL_INFO) == 0) {
        return APP_ID_KERNEL_INFO;
    }
    else if (strcmp(name, APP_NAME_BITMAP) == 0) {
        return APP_ID_BITMAP;
    }
    else {
        return -1;
    }
}

int run_external(char **argv, int argc, bool background) {
    if (!argv || argc <= 0) {
        return -1;
    }

    if (background && argc > 0 && strcmp(argv[argc - 1], "&") == 0) {
        argc--;
    }

    const char *exec_argv[PROC_EXEC_ARGV_MAX + 1];
    int exec_argc = argc;
    if (exec_argc > PROC_EXEC_ARGV_MAX) {
        exec_argc = PROC_EXEC_ARGV_MAX;
    }
    for (int i = 0; i < exec_argc; i++) {
        exec_argv[i] = argv[i];
    }
    exec_argv[exec_argc] = NULL;

    int app_id = app_map(argv[0]);
    if (app_id < 0) {
        printf("command not found\n");
        return -1;
    }

    int pid = fork();
    if (pid < 0) {
        printf("fork failed\n");
        return -1;
    }

    // child
    if (pid == 0) {
        int ret = execv(app_id, exec_argv);
        if (ret < 0) {
            printf("exec failed\n");
            exit();
        }
        exit();
    }

    // parent
    if (!background) {
        int waited = waitpid(pid);
        (void)waited;
    } else {
        printf("[bg] pid=%d\n", pid);
    }
    return pid;
}
