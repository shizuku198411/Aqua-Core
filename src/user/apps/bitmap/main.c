#include "user_syscall.h"
#include "commonlibs.h"

int main(int argc, char **argv) {
    (void) argc;
    (void) argv;

    int total = 0;
    int used = 0;
    for (int i = 0;; i++) {
        int bit = bitmap(i);
        if (bit < 0) {
            break;
        }
        if (bit == 1) {
            used++;
        }
        total++;
    }
    printf("bitmap: total=%d/used=%d/free=%d\n", total, used, (total - used));
    return 0;
}
