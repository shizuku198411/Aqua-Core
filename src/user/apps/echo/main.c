#include "user_syscall.h"
#include "core/commonlibs.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("usage: echo <string>\n");
        return -1;
    }

    // TODO: implement env

    printf("%s\n", argv[1]);
    return 0;
}