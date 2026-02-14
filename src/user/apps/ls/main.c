#include "commonlibs.h"
#include "user_syscall.h"
#include "user_path.h"

int main(int argc, char **argv) {
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

    char path[FS_PATH_MAX];
    const char *path_arg = NULL;
    if (argc == 1) {
        path_arg = ".";
    } else if (argc == 2) {
        if (detail || all_print) {
            path_arg = ".";
        } else {
            path_arg = argv[1];
        }
    } else {
        path_arg = argv[2];
    }

    if (user_path_resolve(path_arg, path, sizeof(path)) < 0) {
        printf("resolve path failed\n");
        return -1;
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
    return 0;
}
