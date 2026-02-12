/*
    application#1: Commandline Shell
*/

#include "commonlibs.h"
#include "syscall.h"
#include "user_syscall.h"

static int split_args(char *line, char **argv, int argv_max) {
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

static int parse_int(const char *s, int *out) {
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

void main(void) {
    while (1) {
prompt:
        printf("aqua-core:$ ");
        char cmdline[64];
        for (int i = 0;; i++) {
            char ch = getchar();
            putchar(ch);
            if (i == sizeof(cmdline) - 1) {
                printf("command too long\n");
                goto prompt;
            }
            else if (ch == '\r') {
                printf("\n");
                cmdline[i] = '\0';
                break;
            }
            else {
                cmdline[i] = ch;
            }
        }

        char *argv[4];
        int argc = split_args(cmdline, argv, 4);
        if (argc == 0) {
            goto prompt;
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

        // ipc_recv
        else if (strcmp(argv[0], "ipc_recv") == 0 && argc == 1) {
            int from_pid = 0;
            int msg = ipc_recv(&from_pid);
            if (msg < 0) {
                printf("ipc_recv failed\n");
            } else {
                printf("ipc from=%d msg=%d\n", from_pid, msg);
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
        else if (strcmp(argv[0], "exit") == 0 && argc == 1) {
            exit();
        }

        else {
            printf("command not found.\n");
        }
    }
}
