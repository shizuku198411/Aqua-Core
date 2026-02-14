#include "user_syscall.h"
#include "commonlibs.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("usage: touch <path>\n");
        return -1;
    }

    int fd = fs_open(argv[1], O_CREAT | O_WRONLY);
    if (fd < 0) {
        printf("touch failed\n");
        return -1;
    }
    fs_close(fd);
    return 0;
}
