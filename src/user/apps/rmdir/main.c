#include "user_syscall.h"
#include "commonlibs.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("usage: rmdir <path>\n");
        return -1;
    }

    if (fs_rmdir(argv[1]) < 0) {
        printf("rmdir failed\n");
        return -1;
    }
    return 0;
}
