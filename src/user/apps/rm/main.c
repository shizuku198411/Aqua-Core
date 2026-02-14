#include "user_syscall.h"
#include "commonlibs.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("usage: rm <path>\n");
        return -1;
    }

    if (fs_unlink(argv[1]) < 0) {
        printf("rm failed\n");
        return -1;
    }
    return 0;
}
