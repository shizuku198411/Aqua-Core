#include "user_syscall.h"
#include "commonlibs.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("usage: mkdir <path>\n");
        return -1;
    }

    if (fs_mkdir(argv[1]) < 0) {
        printf("mkdir failed\n");
        return -1;
    }
    return 0;
}
