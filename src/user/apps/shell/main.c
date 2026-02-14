/*
    application#1: Commandline Shell
*/

#include "shell.h"
#include "commonlibs.h"
#include "user_syscall.h"
#include "commands_fs.h"
#include "commands_proc.h"
#include "commands_sys.h"
#include "syscall.h"

void main(void) {
    history_load();

    while (1) {
prompt:
        printf("\033[34maqua-core\033[0m:$ ");
        char cmdline[CMDLINE_MAX];
        char draft[CMDLINE_MAX];
        int len = 0;
        int draft_len = 0;
        int history_cursor = -1; // -1: not browsing, 0..history_size()-1: browsing
        for (;;) {
            char ch = getchar();

            if (ch == '\r' || ch == '\n') {
                printf("\n");
                cmdline[len] = '\0';
                break;
            }

            // Arrow keys: ESC [ A(up), ESC [ B(down)
            if (ch == 0x1b) {
                char c1 = getchar();
                char c2 = getchar();
                if (c1 == '[' && (c2 == 'A' || c2 == 'B')) {
                    int hsize = history_size();
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
                            redraw_cmdline(cmdline);
                            continue;
                        }
                    }

                    if (history_cursor >= 0) {
                        history_get(history_cursor, cmdline, CMDLINE_MAX);
                        len = str_len(cmdline);
                        redraw_cmdline(cmdline);
                    }
                }
                continue;
            }

            // Handle both BS (0x08) and DEL (0x7f) as erase one character.
            if (ch == '\b' || ch == 0x7f) {
                if (len > 0) {
                    len--;
                    printf("\b \b");
                }
                continue;
            }

            if (len == sizeof(cmdline) - 1) {
                printf("\ncommand too long\n");
                goto prompt;
            }

            cmdline[len++] = ch;
            putchar(ch);
        }

        // push history
        history_push(cmdline);

        char *argv[8];
        int argc = split_args(cmdline, argv, 8);
        if (argc == 0) {
            goto prompt;
        }

        if (strcmp(argv[0], "mkdir") == 0 && argc == 2) {
            shell_cmd_mkdir(argv[1]);
        }

        else if (strcmp(argv[0], "rmdir") == 0 && argc == 2) {
            shell_cmd_rmdir(argv[1]);
        }

        else if (strcmp(argv[0], "touch") == 0 && argc == 2) {
            shell_cmd_touch(argv[1]);
        }

        else if (strcmp(argv[0], "rm") == 0 && argc == 2) {
            shell_cmd_rm(argv[1]);
        }

        else if (strcmp(argv[0], "write") == 0 && argc == 3) {
            shell_cmd_write(argv[1], argv[2]);
        }

        else if (strcmp(argv[0], "cat") == 0 && argc == 2) {
            shell_cmd_cat(argv[1]);
        }

        else if (strcmp(argv[0], "ls") == 0 && (argc == 1 || argc == 2 || argc == 3)) {
            shell_cmd_ls(argc, argv);
        }

        else if (strcmp(argv[0], "ps") == 0 && argc == 1) {
            shell_cmd_ps();
        }

        else if (strcmp(argv[0], "kill") == 0) {
            shell_cmd_kill(argc, argv);
        }

        else if (strcmp(argv[0], "ipc_start") == 0 && argc == 1) {
            shell_cmd_ipc_start();
        }

        else if (strcmp(argv[0], "ipc_send") == 0 && argc == 3) {
            shell_cmd_ipc_send(argv[1], argv[2]);
        }

        else if (strcmp(argv[0], "kernel_info") == 0 && argc == 1) {
            shell_cmd_kernel_info();
        }

        else if (strcmp(argv[0], "history") == 0 && argc == 1) {
            shell_cmd_history();
        }

        else if (strcmp(argv[0], "bitmap") == 0 && argc == 1) {
            shell_cmd_bitmap();
        }

        else if (strcmp(argv[0], "date") == 0 && argc == 1) {
            shell_cmd_gettime();
        }

        else if (strcmp(argv[0], "shutdown") == 0 && argc == 1) {
            history_write();
            shell_cmd_shutdown();
        }

        #ifdef DEBUG
        else if (strcmp(argv[0], "fork_test") == 0 && argc == 1) {
            int pid = fork();
            if (pid < 0) {
                printf("fork failed\n");
                continue;
            }

            if (pid == 0) {
                printf("[child] fork() returned 0\n");
                shell_cmd_ps();
                exit();
            }

            printf("[parent] child pid=%d\n", pid);
            int waited = waitpid(pid);
            printf("[parent] waitpid(%d) => %d\n", pid, waited);
        }

        else if (strcmp(argv[0], "exec_test") == 0 && argc == 1) {
            int pid = fork();
            if (pid < 0)  {
                printf("fork failed\n");
                continue;
            }

            if (pid == 0) {
                printf("[child] fork() returned 0\n");
                printf("[child] exec IPC_RX\n");
                int ret = exec(APP_ID_IPC_RX);
                if (ret < 0) {
                    printf("[child] exec failed\n");
                }
                exit();
            }

            printf("[parent] child pid=%d\n", pid);
        }

        else if (strcmp(argv[0], "dup2_test") == 0 && argc == 1) {
            // create old_fd
            char *old_path = "/tmp/dup2.test";
            shell_cmd_write((const char *)old_path, "dup2_test");
            // open old_path
            int old_fd = fs_open((const char *)old_path, O_RDONLY);

            // case1: invalid new_fd
            if (dup2(old_fd, FS_FD_MAX) == -1) printf("dup2() failed\n");
            // case2: invalid old_fd
            if (dup2(FS_FD_MAX, old_fd) == -1) printf("dup2() failed\n");
            // case3: new_fd same as old_fd
            if (dup2(old_fd, old_fd) == old_fd) printf("dup2() success\n");
            // case4: new_fd not same as old_fd
            int new_fd = old_fd + 1;
            if (dup2(old_fd, new_fd) == new_fd) {
                printf("dup2() success\n");
                // read new_fd
                char buf[64];
                while (1) {
                    int n = fs_read(new_fd, buf, sizeof(buf) - 1);
                    if (n <= 0) {
                        break;
                    }
                    buf[n] = '\0';
                    printf("%s", buf);
                }
                printf("\n");
                fs_close(new_fd);
            } else {
                printf("dup2() failed\n");
            }

            // close old_fd
            fs_close(old_fd);
        }
        #endif

        else {
            printf("command not found.\n");
        }
    }
}
