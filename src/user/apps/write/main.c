#include "user_syscall.h"
#include "commonlibs.h"

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("usage: write <path> <text>\n");
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
