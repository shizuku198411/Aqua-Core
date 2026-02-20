#include "core/commonlibs.h"
#include "user_syscall.h"
#include "fs/fs.h"

#define EDIT_MAX_LINES 256
#define EDIT_LINE_MAX  120
#define VIEW_ROWS      20
#define VIEW_COLS      78
#define CMD_BUF_MAX    32

enum editor_mode {
    MODE_NORMAL = 0,
    MODE_INSERT,
    MODE_COMMAND,
};

enum editor_key {
    KEY_NONE = 0,
    KEY_ENTER = '\n',
    KEY_ESC = 27,
    KEY_BACKSPACE = 127,
    KEY_ARROW_UP = 1001,
    KEY_ARROW_DOWN,
    KEY_ARROW_LEFT,
    KEY_ARROW_RIGHT,
};

static char g_lines[EDIT_MAX_LINES][EDIT_LINE_MAX];
static int g_line_count = 1;
static int g_row;
static int g_col;
static int g_scroll_top;
static int g_modified;
static enum editor_mode g_mode = MODE_NORMAL;
static char g_path[FS_PATH_MAX];
static char g_status[64];
static char g_cmd[CMD_BUF_MAX];
static int g_cmd_len;

static int min_i(int a, int b) {
    return (a < b) ? a : b;
}

static int max_i(int a, int b) {
    return (a > b) ? a : b;
}

static int line_len(int row) {
    int n = 0;
    while (n < EDIT_LINE_MAX && g_lines[row][n] != '\0') {
        n++;
    }
    return n;
}

static void set_status(const char *s) {
    strcpy_s(g_status, sizeof(g_status), s);
}

static int resolve_path(const char *arg, char out[FS_PATH_MAX]) {
    if (!arg || !out || arg[0] == '\0') {
        return -1;
    }
    if (arg[0] == '/') {
        strcpy_s(out, FS_PATH_MAX, arg);
        return 0;
    }

    char cwd[FS_PATH_MAX];
    if (getcwd(cwd) < 0) {
        return -1;
    }
    int cwd_len = 0;
    while (cwd[cwd_len] != '\0' && cwd_len < FS_PATH_MAX) {
        cwd_len++;
    }
    strcpy_s(out, FS_PATH_MAX, cwd);
    if (cwd_len == 0 || cwd[cwd_len - 1] != '/') {
        strcat_s(out, FS_PATH_MAX, "/");
    }
    strcat_s(out, FS_PATH_MAX, arg);
    return 0;
}

static void clear_buffer(void) {
    memset(g_lines, 0, sizeof(g_lines));
    g_line_count = 1;
    g_row = 0;
    g_col = 0;
    g_scroll_top = 0;
    g_modified = 0;
}

static void clamp_cursor(void) {
    if (g_line_count <= 0) {
        g_line_count = 1;
    }
    g_row = max_i(0, min_i(g_row, g_line_count - 1));
    g_col = max_i(0, min_i(g_col, line_len(g_row)));
}

static void ensure_visible(void) {
    if (g_row < g_scroll_top) {
        g_scroll_top = g_row;
    }
    if (g_row >= g_scroll_top + VIEW_ROWS) {
        g_scroll_top = g_row - VIEW_ROWS + 1;
    }
    if (g_scroll_top < 0) {
        g_scroll_top = 0;
    }
}

static int insert_empty_line(int at) {
    if (g_line_count >= EDIT_MAX_LINES || at < 0 || at > g_line_count) {
        return -1;
    }
    for (int i = g_line_count; i > at; i--) {
        memcpy(g_lines[i], g_lines[i - 1], EDIT_LINE_MAX);
    }
    memset(g_lines[at], 0, EDIT_LINE_MAX);
    g_line_count++;
    return 0;
}

static void remove_line(int at) {
    if (at < 0 || at >= g_line_count || g_line_count <= 1) {
        return;
    }
    for (int i = at; i < g_line_count - 1; i++) {
        memcpy(g_lines[i], g_lines[i + 1], EDIT_LINE_MAX);
    }
    memset(g_lines[g_line_count - 1], 0, EDIT_LINE_MAX);
    g_line_count--;
}

static void insert_char(char c) {
    int len = line_len(g_row);
    if (len >= EDIT_LINE_MAX - 1) {
        set_status("line is full");
        return;
    }
    for (int i = len; i >= g_col; i--) {
        g_lines[g_row][i + 1] = g_lines[g_row][i];
    }
    g_lines[g_row][g_col] = c;
    g_col++;
    g_modified = 1;
}

static void split_line(void) {
    if (g_line_count >= EDIT_MAX_LINES) {
        set_status("too many lines");
        return;
    }
    if (insert_empty_line(g_row + 1) < 0) {
        return;
    }

    int len = line_len(g_row);
    int tail = len - g_col;
    if (tail > 0) {
        memcpy(g_lines[g_row + 1], &g_lines[g_row][g_col], (size_t) tail);
        g_lines[g_row + 1][tail] = '\0';
        g_lines[g_row][g_col] = '\0';
    }
    g_row++;
    g_col = 0;
    g_modified = 1;
}

static void backspace_char(void) {
    if (g_col > 0) {
        int len = line_len(g_row);
        for (int i = g_col - 1; i < len; i++) {
            g_lines[g_row][i] = g_lines[g_row][i + 1];
        }
        g_col--;
        g_modified = 1;
        return;
    }

    if (g_row <= 0) {
        return;
    }
    int prev_len = line_len(g_row - 1);
    int cur_len = line_len(g_row);
    if (prev_len + cur_len >= EDIT_LINE_MAX) {
        set_status("line join overflow");
        return;
    }
    memcpy(&g_lines[g_row - 1][prev_len], g_lines[g_row], (size_t) cur_len + 1);
    remove_line(g_row);
    g_row--;
    g_col = prev_len;
    g_modified = 1;
}

static void delete_under_cursor(void) {
    int len = line_len(g_row);
    if (g_col < len) {
        for (int i = g_col; i < len; i++) {
            g_lines[g_row][i] = g_lines[g_row][i + 1];
        }
        g_modified = 1;
        return;
    }

    if (g_row + 1 >= g_line_count) {
        return;
    }
    int next_len = line_len(g_row + 1);
    if (len + next_len >= EDIT_LINE_MAX) {
        set_status("line join overflow");
        return;
    }
    memcpy(&g_lines[g_row][len], g_lines[g_row + 1], (size_t) next_len + 1);
    remove_line(g_row + 1);
    g_modified = 1;
}

static int save_file(void) {
    int fd = fs_open(g_path, O_CREAT | O_WRONLY | O_TRUNC);
    if (fd < 0) {
        set_status("save failed: open");
        return -1;
    }

    for (int i = 0; i < g_line_count; i++) {
        int len = line_len(i);
        if (len > 0 && fs_write(fd, g_lines[i], len) != len) {
            fs_close(fd);
            set_status("save failed: write");
            return -1;
        }
        if (i < g_line_count - 1) {
            char nl = '\n';
            if (fs_write(fd, &nl, 1) != 1) {
                fs_close(fd);
                set_status("save failed: write");
                return -1;
            }
        }
    }

    fs_close(fd);
    g_modified = 0;
    set_status("written");
    return 0;
}

static int load_file(void) {
    clear_buffer();

    int fd = fs_open(g_path, O_RDONLY);
    if (fd < 0) {
        set_status("new file");
        return 0;
    }

    char buf[256];
    int row = 0;
    int col = 0;
    while (1) {
        int n = fs_read(fd, buf, sizeof(buf));
        if (n < 0) {
            fs_close(fd);
            set_status("read failed");
            return -1;
        }
        if (n == 0) {
            break;
        }

        for (int i = 0; i < n; i++) {
            char c = buf[i];
            if (c == '\r') {
                continue;
            }
            if (c == '\n') {
                if (row + 1 >= EDIT_MAX_LINES) {
                    fs_close(fd);
                    set_status("file too large");
                    return -1;
                }
                row++;
                if (row + 1 > g_line_count) {
                    g_line_count = row + 1;
                }
                col = 0;
                continue;
            }
            if (col < EDIT_LINE_MAX - 1) {
                g_lines[row][col++] = c;
                g_lines[row][col] = '\0';
            }
        }
    }

    fs_close(fd);
    if (g_line_count <= 0) {
        g_line_count = 1;
    }
    set_status("opened");
    return 0;
}

static void render_editor(void) {
    ensure_visible();
    clamp_cursor();

    printf("\033[2J");
    printf("\033[H");

    for (int r = 0; r < VIEW_ROWS; r++) {
        int lr = g_scroll_top + r;
        if (lr < g_line_count) {
            int len = line_len(lr);
            int n = min_i(len, VIEW_COLS);
            if (n > 0) {
                char linebuf[VIEW_COLS + 1];
                for (int i = 0; i < n; i++) {
                    char ch = g_lines[lr][i];
                    if (ch < 0x20 || ch > 0x7e) {
                        ch = '?';
                    }
                    linebuf[i] = ch;
                }
                linebuf[n] = '\0';
                printf("%s", linebuf);
            }
        } else {
            printf("~");
        }
        printf("\033[K\r\n");
    }

    const char *m = (g_mode == MODE_INSERT) ? "INSERT" :
                    (g_mode == MODE_COMMAND) ? "COMMAND" : "NORMAL";
    printf("\033[7m");
    printf(" %s %s %s", m, g_modified ? "[+]" : "[-]", g_path);
    printf("\033[K");
    printf("\033[0m\r\n");

    if (g_mode == MODE_COMMAND) {
        printf(":%s", g_cmd);
    } else {
        printf("%s", g_status);
    }
    printf("\033[K");

    int cr = g_row - g_scroll_top + 1;
    int cc = g_col + 1;
    if (cr < 1) {
        cr = 1;
    }
    if (cr > VIEW_ROWS) {
        cr = VIEW_ROWS;
    }
    if (cc < 1) {
        cc = 1;
    }
    if (cc > VIEW_COLS) {
        cc = VIEW_COLS;
    }
    printf("\033[%d;%dH", cr, cc);
}

static int read_key(void) {
    long ch = getchar();
    if (ch < 0) {
        return KEY_NONE;
    }
    if (ch == '\r' || ch == '\n') {
        return KEY_ENTER;
    }
    if (ch == 0x7f || ch == '\b') {
        return KEY_BACKSPACE;
    }
    if (ch != KEY_ESC) {
        return (int) (uint8_t) ch;
    }

    long c1 = getchar();
    if (c1 < 0) {
        return KEY_ESC;
    }
    if (c1 != '[') {
        return KEY_ESC;
    }
    long c2 = getchar();
    if (c2 < 0) {
        return KEY_ESC;
    }
    if (c2 == 'A') return KEY_ARROW_UP;
    if (c2 == 'B') return KEY_ARROW_DOWN;
    if (c2 == 'C') return KEY_ARROW_RIGHT;
    if (c2 == 'D') return KEY_ARROW_LEFT;
    return KEY_ESC;
}

static void move_left(void) {
    if (g_col > 0) {
        g_col--;
    } else if (g_row > 0) {
        g_row--;
        g_col = line_len(g_row);
    }
}

static void move_right(void) {
    int len = line_len(g_row);
    if (g_col < len) {
        g_col++;
    } else if (g_row + 1 < g_line_count) {
        g_row++;
        g_col = 0;
    }
}

static void move_up(void) {
    if (g_row > 0) {
        g_row--;
        g_col = min_i(g_col, line_len(g_row));
    }
}

static void move_down(void) {
    if (g_row + 1 < g_line_count) {
        g_row++;
        g_col = min_i(g_col, line_len(g_row));
    }
}

static int exec_command(void) {
    g_cmd[g_cmd_len] = '\0';
    if (strcmp(g_cmd, "w") == 0) {
        (void) save_file();
        g_mode = MODE_NORMAL;
        return 0;
    }
    if (strcmp(g_cmd, "q") == 0) {
        if (g_modified) {
            set_status("No write since last change (:q!)");
            g_mode = MODE_NORMAL;
            return 0;
        }
        return 1;
    }
    if (strcmp(g_cmd, "q!") == 0) {
        return 1;
    }
    if (strcmp(g_cmd, "wq") == 0) {
        if (save_file() == 0) {
            return 1;
        }
        g_mode = MODE_NORMAL;
        return 0;
    }
    set_status("unknown command");
    g_mode = MODE_NORMAL;
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("usage: edit <path>\n");
        return -1;
    }
    if (resolve_path(argv[1], g_path) < 0) {
        printf("invalid path\n");
        return -1;
    }

    if (load_file() < 0) {
        return -1;
    }

    while (1) {
        render_editor();
        int k = read_key();
        if (k == KEY_NONE) {
            continue;
        }

        if (g_mode == MODE_INSERT) {
            if (k == KEY_ESC) {
                g_mode = MODE_NORMAL;
                set_status("");
            } else if (k == KEY_ENTER) {
                split_line();
            } else if (k == KEY_BACKSPACE) {
                backspace_char();
            } else if (k == KEY_ARROW_LEFT) {
                move_left();
            } else if (k == KEY_ARROW_RIGHT) {
                move_right();
            } else if (k == KEY_ARROW_UP) {
                move_up();
            } else if (k == KEY_ARROW_DOWN) {
                move_down();
            } else if (k >= 0x20 && k <= 0x7e) {
                insert_char((char) k);
            }
            continue;
        }

        if (g_mode == MODE_COMMAND) {
            if (k == KEY_ESC) {
                g_mode = MODE_NORMAL;
                set_status("");
            } else if (k == KEY_BACKSPACE) {
                if (g_cmd_len > 0) {
                    g_cmd_len--;
                    g_cmd[g_cmd_len] = '\0';
                }
            } else if (k == KEY_ENTER) {
                if (exec_command()) {
                    break;
                }
            } else if (k >= 0x20 && k <= 0x7e) {
                if (g_cmd_len < CMD_BUF_MAX - 1) {
                    g_cmd[g_cmd_len++] = (char) k;
                    g_cmd[g_cmd_len] = '\0';
                }
            }
            continue;
        }

        if (k == 'i') {
            g_mode = MODE_INSERT;
            set_status("-- INSERT --");
        } else if (k == ':') {
            g_mode = MODE_COMMAND;
            g_cmd_len = 0;
            g_cmd[0] = '\0';
        } else if (k == 'h' || k == KEY_ARROW_LEFT) {
            move_left();
        } else if (k == 'l' || k == KEY_ARROW_RIGHT) {
            move_right();
        } else if (k == 'k' || k == KEY_ARROW_UP) {
            move_up();
        } else if (k == 'j' || k == KEY_ARROW_DOWN) {
            move_down();
        } else if (k == 'x') {
            delete_under_cursor();
        }
    }

    printf("\033[2J\033[H");
    return 0;
}
