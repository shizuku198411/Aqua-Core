#include "kernel.h"
#include "rtc.h"

static inline uint32_t rtc_read32(uint32_t off) {
    return *(volatile uint32_t *)(GOLDFISH_RTC_BASE + off);
}

int rtc_init(void) {
    volatile uint32_t lo = rtc_read32(RTC_TIME_LOW);
    volatile uint32_t hi = rtc_read32(RTC_TIME_HIGH);
    (void)lo;
    (void)hi;
    return 0;
}

uint64_t rtc_now_ns(void) {
    uint32_t hi1, lo, hi2;
    do {
        hi1 = rtc_read32(RTC_TIME_HIGH);
        lo  = rtc_read32(RTC_TIME_LOW);
        hi2 = rtc_read32(RTC_TIME_HIGH);
    } while (hi1 != hi2);
    return ((uint64_t)hi1 << 32) | lo;
}

static uint32_t udiv64_32(uint64_t n, uint32_t d) {
    uint64_t rem = 0;
    uint64_t q = 0;

    for (int i = 63; i >= 0; i--) {
        rem = (rem << 1) | ((n >> i) & 1ull);
        if (rem >= d) {
            rem -= d;
            q |= (1ull << i);
        }
    }
    return (uint32_t)q;
}

uint32_t rtc_now_sec(void) {
    return udiv64_32(rtc_now_ns(), 1000000000u);
}
