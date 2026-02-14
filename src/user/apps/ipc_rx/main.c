/*
    application#3: ipc_rx
*/

#include "commonlibs.h"
#include "user_syscall.h"

static int parse_int_local(const char *s, int *out) {
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

static int ipc_run_receiver(void) {
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
    return 0;
}

static int ipc_run_sender(const char *pid_s, const char *msg_s) {
    int pid = 0;
    int msg = 0;
    if (parse_int_local(pid_s, &pid) < 0 || parse_int_local(msg_s, &msg) < 0) {
        printf("usage: ipc_rx sender <pid> <msg>\n");
        return -1;
    }

    int ret = ipc_send(pid, msg);
    if (ret == -2) {
        printf("ipc_send failed: receiver mailbox full\n");
        return -1;
    }
    if (ret < 0) {
        printf("ipc_send failed\n");
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return ipc_run_receiver();
    }

    if (strcmp(argv[1], "receiver") == 0) {
        if (argc != 2) {
            printf("usage: ipc_rx receiver\n");
            return -1;
        }
        return ipc_run_receiver();
    }

    if (strcmp(argv[1], "sender") == 0) {
        if (argc != 4) {
            printf("usage: ipc_rx sender <pid> <msg>\n");
            return -1;
        }
        return ipc_run_sender(argv[2], argv[3]);
    }

    printf("usage: ipc_rx receiver | ipc_rx sender <pid> <msg>\n");
    return -1;
}
