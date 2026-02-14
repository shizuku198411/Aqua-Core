#include "commonlibs.h"
#include "user_syscall.h"
#include "rtc.h"

int main(int argc, char **argv) {
    (void) argc;
    (void) argv;
    struct time_spec info;
    int ret = gettime(&info);
    if (ret < 0) {
        return -1;
    }
    
    uint64_t sec = ((uint64_t) info.sec_hi << 32) | info.sec_lo;
    char ts[40];
    unix_time_to_utc_str(sec, ts, sizeof(ts));
    printf("%s\n", ts);

    return 0;
}
