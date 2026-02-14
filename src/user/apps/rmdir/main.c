#include "user_syscall.h"
#include "commonlibs.h"
#include "user_path.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("usage: rmdir <path>\n");
        return -1;
    }

    char target[FS_PATH_MAX];
    if (user_path_resolve(argv[1], target, sizeof(target)) < 0) {
        printf("resolve path failed\n");
        return -1;
    }

    if (fs_rmdir(target) < 0) {
        printf("rmdir failed\n");
        return -1;
    }
    return 0;
}
