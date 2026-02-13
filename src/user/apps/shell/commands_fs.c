#include "commands_fs.h"
#include "shell.h"
#include "commonlibs.h"
#include "user_syscall.h"
#include "fs.h"

void shell_cmd_mkdir(const char *path) {
    if (fs_mkdir(path) < 0) {
        printf("mkdir failed\n");
    }
}

void shell_cmd_rmdir(const char *path) {
    if (fs_rmdir(path) < 0) {
        printf("rmdir failed\n");
    }
}

void shell_cmd_touch(const char *path) {
    int fd = fs_open(path, O_CREAT | O_WRONLY);
    if (fd < 0) {
        printf("touch failed\n");
    } else {
        fs_close(fd);
    }
}

void shell_cmd_rm(const char *path) {
    if (fs_unlink(path) < 0) {
        printf("rm failed\n");
    }
}

void shell_cmd_write(const char *path, const char *text) {
    int fd = fs_open(path, O_CREAT | O_WRONLY | O_TRUNC);
    if (fd < 0) {
        printf("open failed\n");
        return;
    }

    int n = str_len(text);
    int w = fs_write(fd, text, n);
    if (w < 0) {
        printf("write failed\n");
    }
    fs_close(fd);
}

void shell_cmd_cat(const char *path) {
    int fd = fs_open(path, O_RDONLY);
    if (fd < 0) {
        printf("cat failed\n");
        return;
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
}

void shell_cmd_ls(int argc, char **argv) {
    bool detail = false;
    bool all_print = false;

    // parse options
    if (argc == 2 || argc == 3) {
        if (argv[1][0] == '-') {
            const char *argv_1 = argv[1];
            argv_1++;
            while (*argv_1) {
                switch (*argv_1) {
                    case '\0':
                        break;
                    case 'l':
                        detail = true;
                        break;
                    case 'a':
                        all_print = true;
                        break;
                }
                argv_1++;
            }
        }
    }

    char path[FS_NAME_MAX];
    if (argc == 1) {
        strcpy(path, "/");
    } else if (argc == 2) {
        if (detail || all_print) {
            strcpy(path, "/");
        } else {
            strcpy(path, argv[1]);
        }
    } else {
        strcpy(path, argv[2]);
    }

    struct fs_dirent ent;
    if (detail) {
        printf("TYPE\tSIZE\tNAME\n");
        for (int i = 0;; i++) {
            if (fs_readdir(path, i, &ent) < 0) {
                break;
            }
            if (!all_print && ent.name[0] == '.') {
                continue;
            }
            printf("%s\t%d\t", ent.type == FS_TYPE_DIR ? "d" : "f", ent.size);
            if (ent.type == FS_TYPE_DIR) {
                printf("%s/\n", ent.name);
            } else {
                printf("%s\n", ent.name);
            }
        }
    } else {
        int i = 0;
        for (;; i++) {
            if (fs_readdir(path, i, &ent) < 0) {
                break;
            }
            if (!all_print && ent.name[0] == '.') {
                continue;
            }
            if (ent.type == FS_TYPE_DIR) {
                printf("%s/\t", ent.name);
            } else {
                printf("%s\t", ent.name);
            }
            if ((i != 0) && (i % 10 == 0)) {
                printf("\n");
            }
        }
        if (i < 10) {
            printf("\n");
        }
    }
}
