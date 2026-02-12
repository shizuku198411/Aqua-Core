/*
    application#1: Commandline Shell
*/

#include "commonlibs.h"
#include "process.h"
#include "syscall.h"
#include "user_syscall.h"


static void wait_for_pid(int target_pid) {
    while (1) {
        bool found = false;
        for (int i = 0;; i++) {
            int entry = ps(i);
            if (entry < 0) {
                break;
            }

            int pid = entry & 0xffff;
            int state = (entry >> 16) & 0xffff;
            if (pid != target_pid) {
                continue;
            }

            found = true;
            if (state == PROC_EXITED || state == PROC_UNUSED) {
                return;
            }
            break;
        }

        if (!found) {
            return;
        }
    }
}


void main(void) {
    while (1) {
prompt:
        printf("> ");
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

        // execute command
        // == hello ==
        if (strcmp(cmdline, "hello") == 0) {
            printf("Hello World from Shell!\n");
        }

        // == ps ==
        else if (strcmp(cmdline, "ps") == 0) {
            int pid = spawn(APP_ID_PS);
            if (pid < 0) {
                printf("failed to spawn ps\n");
            } else {
                wait_for_pid(pid);
            }
        }

        // == bitmap ==
        else if (strcmp(cmdline, "bitmap") == 0) {
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

        // == exit ==
        else if (strcmp(cmdline, "exit") == 0) {
            exit();
        }

        else if (strcmp(cmdline, "") == 0) {
            goto prompt;
        }
        else {
            printf("command '%s' not found.\n", cmdline);
        }
    }
}
