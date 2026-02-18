#include "kernel/kernel.h"
#include "time/rtc.h"
#include "proc/process.h"
#include "kernel/page_access.h"

static uint64_t udiv64_32_full(uint64_t n, uint32_t d, uint32_t *rem_out) {
    uint64_t q = 0;
    uint64_t rem = 0;

    for (int i = 63; i >= 0; i--) {
        rem = (rem << 1) | ((n >> i) & 1ull);
        if (rem >= d) {
            rem -= d;
            q |= (1ull << i);
        }
    }

    if (rem_out) {
        *rem_out = (uint32_t) rem;
    }

    return q;
}

static int write_user_time_info(struct time_spec *user_ptr, uint64_t sec, uint32_t nsec) {
    if (!user_ptr) {
        return -1;
    }

    uint32_t kbuf_sec_lo = (uint32_t) sec;
    if (copyout(&user_ptr->sec_lo, &kbuf_sec_lo, sizeof(user_ptr->sec_lo)) < 0) {
        return -1;
    }
    uint32_t kbuf_sec_hi = (uint32_t) (sec >> 32);
    if (copyout(&user_ptr->sec_hi, &kbuf_sec_hi, sizeof(user_ptr->sec_hi)) < 0) {
        return -1;
    }
    if (copyout(&user_ptr->nsec, &nsec, sizeof(user_ptr->nsec)) < 0) {
        return -1;
    }
    return 0;
}

void syscall_handle_gettime(struct trap_frame *f) {
    struct time_spec *info_ptr = (struct time_spec *) f->a0;
    uint32_t nsec = 0;
    uint64_t sec = udiv64_32_full(rtc_now_ns(), 1000000000u, &nsec);
    if (write_user_time_info(info_ptr, sec, nsec) < 0) {
        f->a0 = -1;
        return;
    }
    f->a0 = 0;
}

void syscall_handle_sleep(struct trap_frame *f) {
    uint32_t ms = (uint32_t) f->a0;
    f->a0 = process_sleep_current(ms);
}
