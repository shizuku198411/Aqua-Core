#define putchar ac_putchar
#define getchar ac_getchar
#define exit ac_exit
#define printf ac_printf
#define memcpy ac_memcpy
#define memset ac_memset
#define strcpy ac_strcpy
#define strcpy_s ac_strcpy_s
#define strcat ac_strcat
#define strcat_s ac_strcat_s
#define strcmp ac_strcmp
#define unix_time_to_utc_str ac_unix_time_to_utc_str

#include "core/stdtypes.h"
#include "user_path.h"

/*
 * Unit selection criteria:
 * - Path normalization is algorithmic and independent from scheduler/traps.
 * - Dependency on syscall is limited to getcwd() and can be stubbed.
 * - Input/output pairs can be asserted deterministically.
 */

void ac_putchar(char ch) {
    (void) ch;
}

long ac_getchar(void) {
    return -1;
}

void ac_exit(void) {
    for (;;) {
    }
}

static int streq(const char *a, const char *b) {
    int i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return 0;
        }
        i++;
    }
    return a[i] == b[i];
}

// user_path_resolve() depends on getcwd() only for relative path inputs.
int getcwd(char *cwd_path) {
    if (!cwd_path) {
        return -1;
    }
    const char *cwd = "/tmp";
    int i = 0;
    while (cwd[i] != '\0') {
        cwd_path[i] = cwd[i];
        i++;
    }
    cwd_path[i] = '\0';
    return 0;
}

static int test_resolve_absolute_normalize(void) {
    char out[64];
    if (user_path_resolve("/a/./b//../c", out, sizeof(out)) < 0) {
        return __LINE__;
    }
    if (!streq(out, "/a/c")) {
        return __LINE__;
    }
    return 0;
}

static int test_resolve_relative_from_cwd(void) {
    char out[64];
    if (user_path_resolve("log/../hist", out, sizeof(out)) < 0) {
        return __LINE__;
    }
    if (!streq(out, "/tmp/hist")) {
        return __LINE__;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc >= 2 && argv && argv[1] && argv[1][0] != '\0') {
        char c = argv[1][0];
        if (c == '1') {
            return test_resolve_absolute_normalize();
        }
        if (c == '2') {
            return test_resolve_relative_from_cwd();
        }
        return 2;
    }

    int ret = test_resolve_absolute_normalize();
    if (ret != 0) {
        return ret;
    }
    ret = test_resolve_relative_from_cwd();
    if (ret != 0) {
        return ret;
    }
    return 0;
}
