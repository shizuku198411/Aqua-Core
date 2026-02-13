#include "commands_proc.h"
#include "shell.h"
#include "commonlibs.h"
#include "syscall.h"
#include "user_syscall.h"

void shell_cmd_ps(void) {
    printf("PID\tPPID\tAPP\tSTATE\tREASON\n");
    for (int i = 1;; i++) {
        struct ps_info info;
        int ret = ps(i, &info);
        if (ret < 0) {
            break;
        }
        if (info.state == PROC_UNUSED) {
            continue;
        }
        printf("%d\t%d\t%s\t%s\t%s\n",
               info.pid,
               info.parent_pid,
               info.name,
               proc_state_to_string(info.state),
               proc_wait_reason_to_string(info.wait_reason));
    }
}

void shell_cmd_kill(int argc, char **argv) {
    if (argc != 2) {
        printf("target pid is required. kill <pid>\n");
        return;
    }

    int target_pid = 0;
    parse_int(argv[1], &target_pid);
    int ret = kill(target_pid);
    if (ret < 0) {
        printf("pid: %d kill failed. ", target_pid);
        switch (-1 * ret) {
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

void shell_cmd_ipc_start(void) {
    int pid = spawn(APP_ID_IPC_RX);
    if (pid < 0) {
        printf("failed to spawn ipc_rx\n");
    } else {
        printf("ipc_rx pid=%d\n", pid);
    }
}

void shell_cmd_ipc_send(const char *pid_s, const char *msg_s) {
    int pid = 0;
    int msg = 0;
    if (parse_int(pid_s, &pid) < 0 || parse_int(msg_s, &msg) < 0) {
        printf("usage: ipc_send <pid> <msg>\n");
        return;
    }

    int ret = ipc_send(pid, msg);
    if (ret == -2) {
        printf("ipc_send failed: receiver mailbox full\n");
    } else if (ret < 0) {
        printf("ipc_send failed\n");
    }
}
