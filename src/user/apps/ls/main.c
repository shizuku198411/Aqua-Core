#include "core/commonlibs.h"
#include "user_syscall.h"
#include "user_path.h"

static int calc_file_size(uint32_t *size_dst, char *unit_dst, const uint32_t *file_size) {
    uint32_t unit_size;

    // GB
    unit_size = *file_size / (1024 * 1024 * 1024);
    if (unit_size > 0) {
        *size_dst = unit_size;
        strcpy_s(unit_dst, 2, "G");
        return 0;        
    }
    // MB
    unit_size = *file_size / (1024 * 1024);
    if (unit_size > 0) {
        *size_dst = unit_size;
        strcpy_s(unit_dst, 2, "M");
        return 0;        
    }    
    // KB
    unit_size = *file_size / 1024;
    if (unit_size > 0) {
        *size_dst = unit_size;
        strcpy_s(unit_dst, 2, "K");
        return 0;
    }
    return -1;
}

int main(int argc, char **argv) {
    bool detail = false;
    bool all_print = false;
    bool human_readable = false;

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
                    case 'h':
                        human_readable = true;
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
            printf("%s\t", ent.type == FS_TYPE_DIR ? "d" : "-");
            if (human_readable) {
                uint32_t file_size;
                char unit[2];
                if (calc_file_size(&file_size, unit, &ent.size) < 0) {
                    file_size = ent.size;
                    unit[0] = '\0';
                }
                printf("%d%s\t", file_size, unit);
            }
            else {
                printf("%d\t", ent.size);
            }
            if (ent.type == FS_TYPE_DIR) {
                printf("%s/\n", ent.name);
            } else {
                printf("%s\n", ent.name);
            }
        }
    } else {
        int printed = 0;
        for (int i = 0;; i++) {
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
            printed++;
            if ((printed % 10) == 0) {
                printf("\n");
            }
        }
        if ((printed % 10) != 0 || printed == 0) {
            printf("\n");
        }
    }
    return 0;
}
