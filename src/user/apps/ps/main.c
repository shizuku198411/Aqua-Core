#include "user_syscall.h"
#include "core/commonlibs.h"

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
        case PROC_WAIT_SLEEP:
            return "SLEEP";
        case PROC_WAIT_NONE:
            return "";
        default:
            return "UNKNOWN";
    }
}

static void build_args_string(const struct ps_info *info, char *out, size_t out_size) {
    if (!out || out_size == 0) {
        return;
    }
    out[0] = '\0';

    if (!info || info->argc <= 0) {
        return;
    }

    int argc = info->argc;
    if (argc > PROC_EXEC_ARGV_MAX) {
        argc = PROC_EXEC_ARGV_MAX;
    }

    for (int i = 0; i < argc; i++) {
        if (info->argv[i][0] == '\0') {
            continue;
        }
        if (out[0] != '\0') {
            strcat_s(out, out_size, " ");
        }
        strcat_s(out, out_size, info->argv[i]);
    }
}

int main(int argc, char **argv) {
    (void) argc;
    (void) argv;

    printf("PID\tPPID\tSTATE\tREASON\tEXIT\tCMD\n");
    for (int i = 1; i < PROCS_MAX; i++) {
        struct ps_info info;
        int ret = ps(i, &info);
        if (ret < 0) {
            continue;
        }
        if (info.state == PROC_UNUSED) {
            continue;
        }

        char args[PROC_EXEC_ARGV_MAX * (PROC_EXEC_ARG_LEN + 1)];
        build_args_string(&info, args, sizeof(args));
        const char *cmd = (args[0] != '\0') ? args : info.name;

        printf("%d\t%d\t%s\t%s\t%d\t%s\n",
               info.pid,
               info.parent_pid,
               proc_state_to_string(info.state),
               proc_wait_reason_to_string(info.wait_reason),
               info.exit_code,
               cmd);
    }
    return 0;
}
