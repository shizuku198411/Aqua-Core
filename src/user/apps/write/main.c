#include "user_syscall.h"
#include "commonlibs.h"
#include "user_path.h"

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("usage: write <path> <text>\n");
        return -1;
    }

    char target[FS_PATH_MAX];
    if (user_path_resolve(argv[1], target, sizeof(target)) < 0) {
        printf("resolve path failed\n");
        return -1;
    }

    int fd = fs_open(target, O_CREAT | O_WRONLY | O_TRUNC);
    if (fd < 0) {
        printf("open failed\n");
        return -1;
    }

    int n = 0;
    while (argv[2][n] != '\0') {
        n++;
    }
    int w = fs_write(fd, argv[2], n);
    fs_close(fd);
    if (w < 0) {
        printf("write failed\n");
        return -1;
    }
    return 0;
}
