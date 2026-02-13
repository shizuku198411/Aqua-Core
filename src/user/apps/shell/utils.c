#include "shell.h"
#include "commonlibs.h"

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
