#include "user_syscall.h"
#include "commonlibs.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("usage: cat <path>\n");
        return -1;
    }

    char target[FS_PATH_MAX];
    if (argv[1][0] != '/') {
        char cwd_path[FS_PATH_MAX];
        if (getcwd(cwd_path) < 0) {
            printf("get cwd failed\n");
            return -1;
        }
        strcpy_s(target, FS_PATH_MAX, cwd_path);
        if (strcmp(cwd_path, "/") != 0) {
            strcat_s(target, FS_PATH_MAX, "/");
        }
        strcat_s(target, FS_PATH_MAX, argv[1]);
    } else {
        strcpy_s(target, FS_PATH_MAX, argv[1]);
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
