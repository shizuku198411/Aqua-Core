/*
    application#3: ipc_rx
*/

#include "commonlibs.h"
#include "user_syscall.h"

void main(void) {
    printf("ipc_rx started\n");

    while (1) {
        int from_pid = 0;
        int msg = ipc_recv(&from_pid);
        if (msg < 0) {
            printf("ipc_rx: recv failed\n");
            continue;
        }

        printf("ipc_rx: from=%d msg=%d\n", from_pid, msg);
    }
}
