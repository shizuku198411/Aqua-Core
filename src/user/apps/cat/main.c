#include "user_syscall.h"
#include "commonlibs.h"
#include "user_path.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("usage: cat <path>\n");
        return -1;
    }

    char target[FS_PATH_MAX];
    if (user_path_resolve(argv[1], target, sizeof(target)) < 0) {
        printf("resolve path failed\n");
        return -1;
    }

    int fd = fs_open(target, O_RDONLY);
    if (fd < 0) {
        printf("cat failed\n");
        return -1;
    }

    char buf[64];
    while (1) {
        int n = fs_read(fd, buf, sizeof(buf) - 1);
        if (n <= 0) {
            break;
        }
        buf[n] = '\0';
        printf("%s", buf);
    }
    printf("\n");
    fs_close(fd);
    return 0;
}
