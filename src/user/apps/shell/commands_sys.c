#include "commands_sys.h"
#include "commonlibs.h"
#include "user_syscall.h"
#include "shell.h"
#include "fs.h"

void shell_cmd_history(void) {
    history_print();
}

void shell_cmd_stdin_test(void) {
    char buf[64];
    int total = 0;

    while (1) {
        int n = shell_read_input(buf, sizeof(buf) - 1);
        if (n <= 0) {
            break;
        }
        total += n;
        buf[n] = '\0';
        shell_printf("%s", buf);
    }
    shell_printf("\n[stdin_test] bytes=%d\n", total);
}

int shell_cmd_write_file(const char *path, const char *text) {
    if (!path || !text) {
        return -1;
    }

    int fd = fs_open(path, O_CREAT | O_WRONLY | O_TRUNC);
    if (fd < 0) {
        shell_printf("open failed\n");
        return -1;
    }

    int n = str_len(text);
    int w = fs_write(fd, text, n);
    fs_close(fd);
    if (w < 0) {
        shell_printf("write failed\n");
        return -1;
    }
    return 0;
}

__attribute__((noreturn))
void shell_cmd_exit(void) {
    exit();
}
