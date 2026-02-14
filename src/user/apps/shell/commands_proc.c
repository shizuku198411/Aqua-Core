#include "commands_proc.h"
#include "shell.h"
#include "commonlibs.h"
#include "syscall.h"
#include "user_syscall.h"

void shell_cmd_kill(int argc, char **argv) {
    if (argc != 2) {
        shell_printf("target pid is required. kill <pid>\n");
        return;
    }

    int target_pid = 0;
    parse_int(argv[1], &target_pid);
    int ret = kill(target_pid);
    if (ret < 0) {
        shell_printf("pid: %d kill failed. ", target_pid);
        switch (-1 * ret) {
            case 1:
                shell_printf("invalid pid specified\n");
                break;
            case 2:
                shell_printf("process not exist\n");
                break;
            case 3:
                shell_printf("cannot kill init process\n");
                break;
            default:
                shell_printf("unknown error\n");
                break;
        }
    } else {
        shell_printf("pid: %d killed\n", target_pid);
    }
}


void shell_cmd_ipc_send(const char *pid_s, const char *msg_s) {
    int pid = 0;
    int msg = 0;
    if (parse_int(pid_s, &pid) < 0 || parse_int(msg_s, &msg) < 0) {
        shell_printf("usage: ipc_send <pid> <msg>\n");
        return;
    }

    int ret = ipc_send(pid, msg);
    if (ret == -2) {
        shell_printf("ipc_send failed: receiver mailbox full\n");
    } else if (ret < 0) {
        shell_printf("ipc_send failed\n");
    }
}
