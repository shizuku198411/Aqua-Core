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
#include "core/commonlibs.h"

/*
 * Unit selection criteria:
 * - Pure, deterministic logic with no kernel/hardware dependency.
 * - Fast to execute on host without QEMU.
 * - Easy to validate by exact expected outputs.
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

static int test_memcpy_memset(void) {
    char buf[8];
    memset(buf, 0, sizeof(buf));
    if (buf[0] != 0 || buf[7] != 0) {
        return __LINE__;
    }

    const char src[] = "abc";
    memcpy(buf, src, 4);
    if (!streq(buf, "abc")) {
        return __LINE__;
    }
    return 0;
}

static int test_strcpy_strcat(void) {
    char buf[16];
    strcpy_s(buf, sizeof(buf), "aqua");
    strcat_s(buf, sizeof(buf), "_core");
    if (!streq(buf, "aqua_core")) {
        return __LINE__;
    }
    if (strcmp(buf, "aqua_core") != 0) {
        return __LINE__;
    }
    return 0;
}

static int test_unix_time_to_utc_str(void) {
    char out[40];
    if (unix_time_to_utc_str(0, out, sizeof(out)) < 0) {
        return __LINE__;
    }
    if (!streq(out, "Thu Jan 1 00:00:00 UTC 1970")) {
        return __LINE__;
    }

    if (unix_time_to_utc_str(1704067200ull, out, sizeof(out)) < 0) {
        return __LINE__;
    }
    if (!streq(out, "Mon Jan 1 00:00:00 UTC 2024")) {
        return __LINE__;
    }
    return 0;
}

int main(int argc, char **argv) {
    int ret = 0;

    // no host-side stdout in this freestanding-style test binary.
    // single-case mode: argv[1] = '1'|'2'|'3'
    if (argc >= 2 && argv && argv[1] && argv[1][0] != '\0') {
        char c = argv[1][0];
        if (c == '1') {
            return test_memcpy_memset();
        }
        if (c == '2') {
            return test_strcpy_strcat();
        }
        if (c == '3') {
            return test_unix_time_to_utc_str();
        }
        return 2;
    }

    ret = test_memcpy_memset();
    if (ret != 0) return ret;
    ret = test_strcpy_strcat();
    if (ret != 0) return ret;
    ret = test_unix_time_to_utc_str();
    if (ret != 0) return ret;
    return 0;
}
