#include "shell.h"
#include "commonlibs.h"
#include "process.h"
#include "user_syscall.h"

char *proc_state_to_string(int state) {
    switch (state) {
        case PROC_RUNNABLE:
            return "RUN";
        case PROC_WAITTING:
            return "WAIT";
        case PROC_EXITED:
            return "EXIT";
        case PROC_UNUSED:
        default:
            return "UNUSED";
    }
}

char *proc_wait_reason_to_string(int wait_reason) {
    switch (wait_reason) {
        case PROC_WAIT_CHILD_EXIT:
            return "CHILD_EXIT";
        case PROC_WAIT_CONSOLE_INPUT:
            return "CONSOLE_INPUT";
        case PROC_WAIT_IPC_RECV:
            return "IPC_RECV";
        case PROC_WAIT_NONE:
            return "";
        default:
            return "UNKNOWN";
    }
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

void redraw_cmdline(const char *line) {
    // Move to line head, redraw prompt, clear rest of line, then print cmdline.
    printf("\r\033[34maqua-core\033[0m:$ \033[K%s", line);
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
        printf("%d: %s\n", i + 1, history[idx]);
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

static size_t history_serialize(char *out, size_t out_cap) {
    size_t pos = 0;
    for (int i = 0; i < history_count; i++) {
        int idx = (history_head + i) % HISTORY_MAX;
        const char *s = history[idx];
        while (*s) {
            char c = *s++;
            if (c == '\n' || c == '\\') {
                if (pos + 2 >= out_cap) {
                    return pos;
                }
                out[pos++] = '\\';
                out[pos++] = (c == '\n') ? 'n' : '\\';
            } else {
                if (pos + 1 >= out_cap) {
                    return pos;
                }
                out[pos++] = c;
            }
        }
        if (pos + 1 >= out_cap) {
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

    char buf[HISTORY_MAX * CMDLINE_MAX * 2];
    size_t n = history_serialize(buf, sizeof(buf));
    int w = fs_write(fd, buf, n);
    if (w < 0) {
        printf("write failed\n");
    }
    fs_close(fd);
}
