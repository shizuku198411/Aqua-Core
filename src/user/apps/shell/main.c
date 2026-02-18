/*
    application#1: Commandline Shell
*/

#include "shell.h"
#include "core/commonlibs.h"
#include "user_syscall.h"
#include "commands_sys.h"
#include "user/syscall.h"
#include "fs/fs.h"

#define NUM_BUILTIN_CMD 6
// shell built-in command
const char builtin_cmd[NUM_BUILTIN_CMD][FS_NAME_MAX] = {
    "history",
    "pwd",
    "cd",
    "net_send_raw",
    "net_recv_raw",
    "exit"
};

char shell_app_names[FS_MAX_NODES][FS_NAME_MAX];

static int min_int(int a, int b) {
    return (a < b) ? a : b;
}

static int first_token_end(const char *s, int len) {
    int i = 0;
    while (i < len && s[i] != ' ' && s[i] != '\t') {
        i++;
    }
    return i;
}

static int common_prefix_len(const char *a, const char *b) {
    int i = 0;
    while (a[i] != '\0' && b[i] != '\0' && a[i] == b[i]) {
        i++;
    }
    return i;
}

static void replace_first_token(char *cmdline,
                                int *len,
                                int *cursor,
                                int old_token_len,
                                const char *new_token,
                                int new_token_len) {
    int old_len = *len;
    int tail_len = old_len - old_token_len;
    int new_len = new_token_len + tail_len;
    if (new_len >= CMDLINE_MAX) {
        return;
    }

    if (new_token_len > old_token_len) {
        for (int i = old_len; i >= old_token_len; i--) {
            cmdline[i + (new_token_len - old_token_len)] = cmdline[i];
        }
    } else if (new_token_len < old_token_len) {
        for (int i = old_token_len; i <= old_len; i++) {
            cmdline[i - (old_token_len - new_token_len)] = cmdline[i];
        }
    }

    for (int i = 0; i < new_token_len; i++) {
        cmdline[i] = new_token[i];
    }

    *len = new_len;
    *cursor = new_token_len;
    cmdline[*len] = '\0';
}

static void read_bin_dir(void) {
    // clear
    memset(shell_app_names, 0, FS_MAX_NODES * FS_NAME_MAX);
    struct fs_dirent ent;
    int i;
    for (i = 0;; i++) {
        if (fs_readdir("/bin", i, &ent) < 0) {
            break;
        }
        strcpy_s(shell_app_names[i], FS_NAME_MAX, ent.name);
    }
    for (int j = 0; j < NUM_BUILTIN_CMD; j++) {
        strcpy_s(shell_app_names[i], FS_NAME_MAX, builtin_cmd[j]);
        i++;
    }
}

static int complete_app_name(char *cmdline, int *len, int *cursor) {
    if (!cmdline || !len || !cursor) {
        return 0;
    }

    read_bin_dir();

    int token_end = first_token_end(cmdline, *len);
    if (*cursor > token_end || *cursor <= 0) {
        return 0;
    }

    int prefix_len = *cursor;
    int matches[sizeof(shell_app_names) / sizeof(shell_app_names[0])];
    int match_count = 0;
    int app_count = (int)(sizeof(shell_app_names) / sizeof(shell_app_names[0]));

    for (int i = 0; i < app_count; i++) {
        int same = 1;
        for (int j = 0; j < prefix_len; j++) {
            if (shell_app_names[i][j] != cmdline[j]) {
                same = 0;
                break;
            }
        }
        if (same) {
            matches[match_count++] = i;
        }
    }

    if (match_count == 0) {
        return 0;
    }

    if (match_count == 1) {
        const char *m = shell_app_names[matches[0]];
        int mlen = str_len(m);
        replace_first_token(cmdline, len, cursor, token_end, m, mlen);
        return 1;
    }

    int lcp = str_len(shell_app_names[matches[0]]);
    for (int i = 1; i < match_count; i++) {
        lcp = min_int(lcp, common_prefix_len(shell_app_names[matches[0]],
                                             shell_app_names[matches[i]]));
    }
    if (lcp > token_end) {
        replace_first_token(cmdline, len, cursor, token_end, shell_app_names[matches[0]], lcp);
        return 1;
    }

    return 0;
}

int main(int shell_argc, char **shell_argv) {
    (void) shell_argc;
    (void) shell_argv;
    history_load();
    char cwd_path[FS_PATH_MAX];

    while (1) {
prompt:
        if (getcwd(cwd_path) < 0) {
            strcpy_s(cwd_path, FS_PATH_MAX, "unkown");
        }
        printf("%saqua-core%s:%s%s%s$ ", CONSOLE_BLUE, CONSOLE_WHITE, CONSOLE_BLUE, cwd_path, CONSOLE_WHITE);
        char cmdline[CMDLINE_MAX];
        char draft[CMDLINE_MAX];
        int len = 0;
        int cursor = 0;
        int draft_len = 0;
        int history_cursor = -1; // -1: not browsing, 0..history_size()-1: browsing
        for (;;) {
            char ch = getchar();

            if (ch == '\r' || ch == '\n') {
                printf("\n");
                cmdline[len] = '\0';
                break;
            }

            // Arrow keys: ESC [ A(up), ESC [ B(down), ESC [ C(right), ESC [ D(left)
            // Delete key: ESC [ 3 ~
            if (ch == 0x1b) {
                char c1 = getchar();
                char c2 = getchar();
                if (c1 == '[' && (c2 == 'A' || c2 == 'B' || c2 == 'C' || c2 == 'D' || c2 == '3')) {
                    int hsize = history_size();

                    if (c2 == 'C') { // right
                        if (cursor < len) {
                            cursor++;
                            redraw_cmdline(cmdline, cursor, cwd_path);
                        }
                        continue;
                    } else if (c2 == 'D') { // left
                        if (cursor > 0) {
                            cursor--;
                            redraw_cmdline(cmdline, cursor, cwd_path);
                        }
                        continue;
                    } else if (c2 == '3') { // delete
                        char c3 = getchar();
                        if (c3 == '~' && cursor < len) {
                            for (int i = cursor; i < len - 1; i++) {
                                cmdline[i] = cmdline[i + 1];
                            }
                            len--;
                            cmdline[len] = '\0';
                            redraw_cmdline(cmdline, cursor, cwd_path);
                        }
                        continue;
                    }

                    if (hsize <= 0) {
                        continue;
                    }

                    if (c2 == 'A') { // up: older
                        if (history_cursor == -1) {
                            for (int i = 0; i < len; i++) {
                                draft[i] = cmdline[i];
                            }
                            draft[len] = '\0';
                            draft_len = len;
                            history_cursor = hsize - 1;
                        } else if (history_cursor > 0) {
                            history_cursor--;
                        }
                    } else { // down: newer
                        if (history_cursor == -1) {
                            continue;
                        }

                        if (history_cursor < hsize - 1) {
                            history_cursor++;
                        } else {
                            // back to draft
                            history_cursor = -1;
                            for (int i = 0; i < draft_len; i++) {
                                cmdline[i] = draft[i];
                            }
                            cmdline[draft_len] = '\0';
                            len = draft_len;
                            cursor = len;
                            redraw_cmdline(cmdline, cursor, cwd_path);
                            continue;
                        }
                    }

                    if (history_cursor >= 0) {
                        history_get(history_cursor, cmdline, CMDLINE_MAX);
                        len = str_len(cmdline);
                        cursor = len;
                        redraw_cmdline(cmdline, cursor, cwd_path);
                    }
                }
                continue;
            }

            // Tab completion (app names only)
            if (ch == '\t') {
                if (complete_app_name(cmdline, &len, &cursor)) {
                    redraw_cmdline(cmdline, cursor, cwd_path);
                }
                continue;
            }

            // Handle both BS (0x08) and DEL (0x7f) as erase one character.
            if (ch == '\b' || ch == 0x7f) {
                if (cursor > 0) {
                    for (int i = cursor - 1; i < len - 1; i++) {
                        cmdline[i] = cmdline[i + 1];
                    }
                    len--;
                    cursor--;
                    cmdline[len] = '\0';
                    redraw_cmdline(cmdline, cursor, cwd_path);
                }
                continue;
            }

            if (len == sizeof(cmdline) - 1) {
                printf("\ncommand too long\n");
                goto prompt;
            }

            if (cursor == len) {
                cmdline[len++] = ch;
                cursor = len;
                putchar(ch);
            } else {
                for (int i = len; i >= cursor; i--) {
                    cmdline[i + 1] = cmdline[i];
                }
                cmdline[cursor] = ch;
                len++;
                cursor++;
                redraw_cmdline(cmdline, cursor, cwd_path);
            }
        }

        // push history
        history_push(cmdline);

        char *raw_argv[8];
        int raw_argc = split_args(cmdline, raw_argv, 8);
        if (raw_argc == 0) {
            goto prompt;
        }

        // parse redirection
        char *in_path = NULL;
        char *out_path = NULL;
        char *argv[8];
        int argc = 0;
        if (parse_redirection(raw_argv, raw_argc, argv, &argc, &in_path, &out_path) < 0) {
            printf("redirection syntax error\n");
            goto prompt;
        }
        if (argc == 0) {
            printf("command not found.\n");
            goto prompt;
        }

        // prepare fd
        bool stdin_redirected = false;
        bool stdout_redirected = false;
        if (in_path) {
            int fd_in = fs_open(in_path, O_RDONLY);
            if (fd_in < 0) {
                printf("file open failed\n");
                goto prompt;
            }
            if (fd_in != 0) {
                if (dup2(fd_in, STDIN) < 0) {
                    printf("dup2 failed\n");
                    fs_close(fd_in);
                    goto prompt;
                }
                fs_close(fd_in);
            }
            stdin_redirected = true;
        }
        if (out_path) {
            int fd_out = fs_open(out_path, O_WRONLY | O_CREAT | O_TRUNC);
            if (fd_out < 0) {
                printf("file open failed\n");
                goto prompt;
            }
            if (fd_out != 1) {
                if (dup2(fd_out, STDOUT) < 0) {
                    printf("dup2 failed\n");
                    fs_close(fd_out);
                    goto prompt;
                }
                fs_close(fd_out);
            }
            stdout_redirected = true;
        }

        if (strcmp(argv[0], "history") == 0 && argc == 1) {
            shell_cmd_history();
        }

        else if (strcmp(argv[0], "cd") == 0) {
            if (is_background(argv, argc)) {
                printf("cd: background execution is not supported\n");
            } else if (argc != 2) {
                printf("usage: cd <path>\n");
            } else if (shell_cmd_cd(argv[1]) < 0) {
                printf("chdir failed\n");
            }
        }

        else if (strcmp(argv[0], "pwd") == 0) {
            if (shell_cmd_pwd() < 0) {
                printf("pwd failed\n");
            }
        }

        else if (strcmp(argv[0], "net_send_raw") == 0) {
            if (shell_cmd_net_send_raw(argc, argv) < 0) {
                printf("net_send_raw failed\n");
            }
        }

        else if (strcmp(argv[0], "net_recv_raw") == 0) {
            int max_bytes = 1600;
            if (argc >= 2) {
                if (parse_int(argv[1], &max_bytes) < 0) {
                    printf("usage: net_recv_raw [max_bytes]\n");
                    goto done_cmd;
                }
            }
            if (shell_cmd_net_recv_raw(max_bytes) < 0) {
                printf("net_recv_raw failed\n");
            }
        }

        else if (strcmp(argv[0], "exit") == 0 && argc == 1) {
            history_write();
            shell_cmd_exit();
        }

        else {
            bool background = is_background(argv, argc);
            run_external(argv, argc, background);
        }
done_cmd:

        if (stdout_redirected) {
            int fd_console_out = fs_open("/dev/console", O_WRONLY);
            if (fd_console_out >= 0) {
                (void) dup2(fd_console_out, STDOUT);
                if (fd_console_out != STDOUT) {
                    fs_close(fd_console_out);
                }
            }
        }
        if (stdin_redirected) {
            int fd_console_in = fs_open("/dev/console", O_RDONLY);
            if (fd_console_in >= 0) {
                (void) dup2(fd_console_in, STDIN);
                if (fd_console_in != STDIN) {
                    fs_close(fd_console_in);
                }
            }
        }
    }

    return 0;
}
