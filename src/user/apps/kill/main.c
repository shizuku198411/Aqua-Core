#include "user_syscall.h"
#include "commonlibs.h"

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

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("target pid is required. kill <pid>\n");
        return -1;
    }

    int target_pid = 0;
    if (parse_int_local(argv[1], &target_pid) < 0) {
        printf("invalid pid specified\n");
        return -1;
    }

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
        return -1;
    }

    printf("pid: %d killed\n", target_pid);
    return 0;
}
