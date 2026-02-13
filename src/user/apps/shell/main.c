/*
    application#1: Commandline Shell
*/

#include "shell.h"
#include "commonlibs.h"
#include "syscall.h"
#include "user_syscall.h"
#include "fs.h"


void main(void) {
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

        // mkdir
        else if (strcmp(argv[0], "mkdir") == 0 && argc == 2) {
            if (fs_mkdir(argv[1]) < 0) {
                printf("mkdir failed\n");
            }
        }

        // rmdir
        else if (strcmp(argv[0], "rmdir") == 0 && argc == 2) {
            if (fs_rmdir(argv[1]) < 0) {
                printf("rmdir failed\n");
            }
        }

        // touch
        else if (strcmp(argv[0], "touch") == 0 && argc == 2) {
            int fd = fs_open(argv[1], O_CREAT | O_WRONLY);
            if (fd < 0) {
                printf("touch failed\n");
            } else {
                fs_close(fd);
            }
        }

        // rm
        else if (strcmp(argv[0], "rm") == 0 && argc == 2) {
            if (fs_unlink(argv[1]) < 0) {
                printf("rm failed\n");
            }
        }

        // write
        else if (strcmp(argv[0], "write") == 0 && argc == 3) {
            int fd = fs_open(argv[1], O_CREAT | O_WRONLY | O_TRUNC);
            if (fd < 0) {
                printf("open failed\n");
            } else {
                int n = str_len(argv[2]);
                int w = fs_write(fd, argv[2], n);
                if (w < 0) {
                    printf("write failed\n");
                }
                fs_close(fd);
            }
        }

        // cat
        else if (strcmp(argv[0], "cat") == 0 && argc == 2) {
            int fd = fs_open(argv[1], O_RDONLY);
            if (fd < 0) {
                printf("cat failed\n");
            } else {
                char buf[64];
                while (1) {
                    int n = fs_read(fd, buf, sizeof(buf) - 1);
                    if (n <= 0) {
                        break;
                    }
                    buf[n] = '\0';
                    printf("%s", buf);
                }
                printf("\n");
                fs_close(fd);
            }
        }

        // ls
        else if (strcmp(argv[0], "ls") == 0 && (argc == 1 || argc == 2)) {
            const char *path = (argc == 2) ? argv[1] : "/";
            struct fs_dirent ent;

            printf("TYPE\tSIZE\tNAME\n");
            for (int i = 0;; i++) {
                if (fs_readdir(path, i, &ent) < 0) {
                    break;
                }
                printf("%s\t%d\t",
                       ent.type == FS_TYPE_DIR ? "d" : "f",
                       ent.size
                );
                if (ent.type == FS_TYPE_DIR) {
                    printf("%s/\n", ent.name);
                } else {
                    printf("%s\n", ent.name);
                }
            }
        }

        // kernel_info
        else if (strcmp(argv[0], "kernel_info") == 0 && argc == 1) {
            struct kernel_info info;
            int ret = kernel_info(&info);
            if (ret < 0) {
                printf("kernel_info failed\n");
            } else {
                printf("version       : %s\n", info.version);
                printf("total pages   : %d\n", info.total_pages);
                printf("page size     : %d bytes\n", info.page_size);
                printf("kernel base   : 0x%x\n", info.kernel_base);
                printf("user base     : 0x%x\n", info.user_base);
                printf("proc max      : %d\n", info.proc_max);
                printf("kernel stack  : %d bytes/proc\n", info.kernel_stack_bytes);
                printf("time slice    : %d ticks\n", info.time_slice_ticks);
                printf("timer interval: %d ms\n", info.timer_interval_ms);
                printf("ramfs node max: %d\n", info.ramfs_node_max);
                printf("ramfs size max: %d\n", info.ramfs_size_max);
            }
        }

        // history
        else if (strcmp(argv[0], "history") == 0) {
            history_print();
        }

        // ps
        else if (strcmp(argv[0], "ps") == 0 && argc == 1) {
            int pid = spawn(APP_ID_PS);
            if (pid < 0) {
                printf("failed to spawn ps\n");
            }
            else {
                int waited = waitpid(pid);
                if (waited < 0) {
                    printf("waitpid failed\n");
                }
            }
        }

        // kill
        else if (strcmp(argv[0], "kill") == 0) {
            if (argc != 2) {
                printf("target pid is required. kill <pid>\n");
            } else {
                int target_pid = 0;
                parse_int(argv[1], &target_pid);
                int ret = kill(target_pid);
                if (ret < 0) {
                    printf("pid: %d kill failed. ", target_pid);
                    switch(-1 * ret) {
                        case 1:
                            printf("invalid pid specified\n");
                            break;
                        case 2:
                            printf("process not exist\n");
                            break;
                        case 3:
                            printf("cannot kill init process\n");
                            break;
                        default:
                            printf("unknown error\n");
                            break;
                    }
                } else {
                    printf("pid: %d killed\n", target_pid);
                }
            }
        }

        // ipc_start
        else if (strcmp(argv[0], "ipc_start") == 0 && argc == 1) {
            int pid = spawn(APP_ID_IPC_RX);
            if (pid < 0) {
                printf("failed to spawn ipc_rx\n");
            } else {
                printf("ipc_rx pid=%d\n", pid);
            }
        }

        // ipc_send
        else if (strcmp(argv[0], "ipc_send") == 0 && argc == 3) {
            int pid = 0;
            int msg = 0;
            if (parse_int(argv[1], &pid) < 0 || parse_int(argv[2], &msg) < 0) {
                printf("usage: ipc_send <pid> <msg>\n");
                continue;
            }

            int ret = ipc_send(pid, msg);
            if (ret == -2) {
                printf("ipc_send failed: receiver mailbox full\n");
            } else if (ret < 0) {
                printf("ipc_send failed\n");
            }
        }

        // bitmap
        else if (strcmp(argv[0], "bitmap") == 0 && argc == 1) {
            int total = 0;
            int used = 0;
            for (int i = 0;; i++) {
                int bit = bitmap(i);
                if (bit < 0) {
                    break;
                }
                if (bit == 1) used++;
                total++;
            }
            printf("bitmap: total=%d/used=%d/free=%d\n", total, used, (total-used));
        }

        // exit
        else if (strcmp(argv[0], "shutdown") == 0 && argc == 1) {
            exit();
        }

        else {
            printf("command not found.\n");
        }
    }
}
