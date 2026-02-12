/*
    application#2: ps
*/

#include "commonlibs.h"
#include "process.h"
#include "user_syscall.h"

static const char *proc_state_to_string(int state) {
    switch (state) {
        case PROC_RUNNABLE:
            return "RUNNABLE";
        case PROC_WAITTING:
            return "WAITING";
        case PROC_EXITED:
            return "EXITED";
        case PROC_UNUSED:
        default:
            return "UNUSED";
    }
}

void main(void) {
    printf("PID STATE\n");

    for (int i = 0;; i++) {
        int entry = ps(i);
        if (entry < 0) {
            break;
        }

        int pid = entry & 0xffff;
        int state = (entry >> 16) & 0xffff;

        if (state == PROC_UNUSED) {
            continue;
        }

        printf("%d %s\n", pid, proc_state_to_string(state));
    }

    exit();
}
