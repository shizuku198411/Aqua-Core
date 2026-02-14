#include "user_syscall.h"
#include "commonlibs.h"
#include "user_path.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("usage: touch <path>\n");
        return -1;
    }

    char target[FS_PATH_MAX];
    if (user_path_resolve(argv[1], target, sizeof(target)) < 0) {
        printf("resolve path failed\n");
        return -1;
    }

    int fd = fs_open(target, O_CREAT | O_WRONLY);
    if (fd < 0) {
        printf("touch failed\n");
        return -1;
    }
    fs_close(fd);
    return 0;
}
