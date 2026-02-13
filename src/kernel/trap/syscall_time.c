#include "kernel.h"
#include "rtc.h"

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

static void write_user_time_info(struct time_spec *user_ptr, uint64_t sec, uint32_t nsec) {
    if (!user_ptr) {
        return;
    }

    uint32_t sstatus = READ_CSR(sstatus);
    WRITE_CSR(sstatus, sstatus | SSTATUS_SUM);

    user_ptr->sec_lo = (uint32_t) sec;
    user_ptr->sec_hi = (uint32_t) (sec >> 32);
    user_ptr->nsec = nsec;

    WRITE_CSR(sstatus, sstatus);
}

void syscall_handle_gettime(struct trap_frame *f) {
    struct time_spec *info_ptr = (struct time_spec *) f->a0;
    uint32_t nsec = 0;
    uint64_t sec = udiv64_32_full(rtc_now_ns(), 1000000000u, &nsec);
    write_user_time_info(info_ptr, sec, nsec);
    f->a0 = 0;
}
