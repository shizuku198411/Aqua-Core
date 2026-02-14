#include "commands_fs.h"
#include "shell.h"
#include "commonlibs.h"
#include "user_syscall.h"
#include "fs.h"

void shell_cmd_mkdir(const char *path) {
    if (fs_mkdir(path) < 0) {
        shell_printf("mkdir failed\n");
    }
}

void shell_cmd_rmdir(const char *path) {
    if (fs_rmdir(path) < 0) {
        shell_printf("rmdir failed\n");
    }
}

void shell_cmd_touch(const char *path) {
    int fd = fs_open(path, O_CREAT | O_WRONLY);
    if (fd < 0) {
        shell_printf("touch failed\n");
    } else {
        fs_close(fd);
    }
}

void shell_cmd_rm(const char *path) {
    if (fs_unlink(path) < 0) {
        shell_printf("rm failed\n");
    }
}

void shell_cmd_write(const char *path, const char *text) {
    int fd = fs_open(path, O_CREAT | O_WRONLY | O_TRUNC);
    if (fd < 0) {
        shell_printf("open failed\n");
        return;
    }

    int n = str_len(text);
    int w = fs_write(fd, text, n);
    if (w < 0) {
        shell_printf("write failed\n");
    }
    fs_close(fd);
}

void shell_cmd_cat(const char *path) {
    int fd = fs_open(path, O_RDONLY);
    if (fd < 0) {
        shell_printf("cat failed\n");
        return;
    }

    char buf[64];
    while (1) {
        int n = fs_read(fd, buf, sizeof(buf) - 1);
        if (n <= 0) {
            break;
        }
        buf[n] = '\0';
        shell_printf("%s", buf);
    }
    shell_printf("\n");
    fs_close(fd);
}
