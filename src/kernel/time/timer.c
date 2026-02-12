#include "kernel.h"
#include "timer.h"
#include "stdtypes.h"


void enable_timer_interrupt(void) {
    uint32_t sie = READ_CSR(sie);
    sie |= (1 << 5);
    WRITE_CSR(sie, sie);

    uint32_t sstatus = READ_CSR(sstatus);
    sstatus |= (1 << 1);
    WRITE_CSR(sstatus, sstatus);
}


static inline uint64_t rdtime(void) {
    uint32_t lo, hi;
    __asm__ __volatile__ (
        "rdtimeh %0\n"
        "rdtime %1\n"
        : "=r"(hi), "=r"(lo)
    );
    return ((uint64_t) hi << 32) | lo;
}


static inline void wrtimecmp(uint64_t val) {
    uint32_t lo = val & 0xffffffff;
    uint32_t hi = val >> 32;
    __asm__ __volatile__ (
        "csrw stimecmp, %0\n"
        "csrw stimecmph, %1\n"
        :
        : "r"(lo), "r"(hi)
    );
}


void timer_set_next() {
    uint64_t now = rdtime();
    wrtimecmp(now + TIMER_INTERVAL);
}
