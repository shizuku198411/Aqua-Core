#include "user_syscall.h"
#include "commonlibs.h"
#include "fs.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("usage: mkdir <path>\n");
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

    if (fs_mkdir(target) < 0) {
        printf("mkdir failed\n");
        return -1;
    }
    return 0;
}
