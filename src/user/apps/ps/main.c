/*
    application#2: ps
*/

#include "commonlibs.h"
#include "process.h"
#include "user_syscall.h"

static const char *proc_state_to_string(int state) {
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

static const char *proc_wait_reason_to_string(int wait_reason) {
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

void main(void) {
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

        printf("%d\t%d\t%s\t%s\t%s\n", info.pid, info.parent_pid, info.name,
               proc_state_to_string(info.state), proc_wait_reason_to_string(info.wait_reason));
    }

    exit();
}
