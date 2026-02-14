#include "user_syscall.h"
#include "commonlibs.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("usage: touch <path>\n");
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

    int fd = fs_open(target, O_CREAT | O_WRONLY);
    if (fd < 0) {
        printf("touch failed\n");
        return -1;
    }
    fs_close(fd);
    return 0;
}
