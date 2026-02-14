#include "user_path.h"
#include "user_syscall.h"
#include "commonlibs.h"
#include "fs.h"

static int pop_component(char *path, int *len) {
    if (!path || !len) {
        return -1;
    }
    if (*len <= 1) {
        *len = 1;
        path[1] = '\0';
        return 0;
    }

    int i = *len - 1;
    while (i > 0 && path[i] != '/') {
        i--;
    }
    if (i == 0) {
        *len = 1;
        path[1] = '\0';
        return 0;
    }

    *len = i;
    path[*len] = '\0';
    return 0;
}

int user_path_resolve(const char *input_path, char *out_path, size_t out_size) {
    if (!input_path || !out_path || out_size == 0) {
        return -1;
    }
    out_path[0] = '\0';

    char abs[FS_PATH_MAX];
    abs[0] = '\0';

    if (input_path[0] == '/') {
        strcpy_s(abs, FS_PATH_MAX, input_path);
    } else {
        char cwd[FS_PATH_MAX];
        if (getcwd(cwd) < 0) {
            return -1;
        }
        strcpy_s(abs, FS_PATH_MAX, cwd);
        if (strcmp(abs, "/") != 0) {
            strcat_s(abs, FS_PATH_MAX, "/");
        }
        strcat_s(abs, FS_PATH_MAX, input_path);
    }

    char norm[FS_PATH_MAX];
    norm[0] = '/';
    norm[1] = '\0';
    int norm_len = 1;

    int i = 0;
    while (abs[i] == '/') {
        i++;
    }

    while (1) {
        char token[FS_NAME_MAX];
        int token_len = 0;

        while (abs[i] != '\0' && abs[i] != '/') {
            if (token_len < FS_NAME_MAX - 1) {
                token[token_len++] = abs[i];
            }
            i++;
        }
        token[token_len] = '\0';

        if (token_len > 0) {
            if (strcmp(token, ".") == 0) {
                // no-op
            } else if (strcmp(token, "..") == 0) {
                pop_component(norm, &norm_len);
            } else {
                if (norm_len > 1) {
                    if (norm_len + 1 >= FS_PATH_MAX) {
                        return -1;
                    }
                    norm[norm_len++] = '/';
                }
                for (int j = 0; j < token_len; j++) {
                    if (norm_len + 1 >= FS_PATH_MAX) {
                        return -1;
                    }
                    norm[norm_len++] = token[j];
                }
                norm[norm_len] = '\0';
            }
        }

        if (abs[i] == '\0') {
            break;
        }
        while (abs[i] == '/') {
            i++;
        }
    }

    strcpy_s(out_path, out_size, norm);
    return 0;
}

