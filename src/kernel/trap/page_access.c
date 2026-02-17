#include "kernel/kernel.h"
#include "core/stdtypes.h"
#include "core/commonlibs.h"
#include "proc/process.h"
#include "mm/memory.h"

extern struct process *current_proc;

uint32_t sum_enter(void) {
    uint32_t sstatus = READ_CSR(sstatus);
    WRITE_CSR(sstatus, sstatus | SSTATUS_SUM);
    return sstatus;
}

void sum_leave(uint32_t saved) {
    WRITE_CSR(sstatus, saved);
}

// check the address is valid user range
static int user_range_ok(const void *uaddr, size_t n) {
    if (!current_proc || !uaddr) {
        return -1;
    }
    if (n == 0) {
        return -1;
    }

    uintptr_t lo = (uintptr_t) uaddr;
    uintptr_t hi = lo + n - 1;
    if (hi < lo) {
        return -1;  // overflow
    }

    uintptr_t user_lo = USER_BASE;
    uintptr_t user_hi = USER_BASE + (uintptr_t) current_proc->user_pages * PAGE_SIZE - 1;

    if (lo < user_lo || hi > user_hi) {
        return -1;
    }
    return 0;
}

int copyinstr(char *kdst, const char *usrc, size_t max) {
    if (!kdst || !usrc || max == 0) {
        return -1;
    }

    uint32_t sstatus = sum_enter();
    int ret = -1;
    for (size_t i = 0; i < max; i++) {
        if (user_range_ok(usrc + i, 1) < 0) {
            goto leave;
        }
        char c = usrc[i];
        kdst[i] = c;
        if (c == '\0') {
            ret = 0;
            goto leave;
        }
    }
leave:
    sum_leave(sstatus);
    return ret;
}

int copyin(void *kdst, const void *usrc, size_t n) {
    if (!kdst) {
        return -1;
    }
    if (n == 0) {
        return 0;
    }
    if (user_range_ok(usrc, n) < 0) {
        return -1;
    }

    uint32_t sstatus = sum_enter();
    memcpy(kdst, usrc, n);
    sum_leave(sstatus);
    return 0;
}

int copyout(void *udst, const void *ksrc, size_t n) {
    if (!ksrc) {
        return -1;
    }
    if (n == 0) {
        return 0;
    }
    if (user_range_ok(udst, n) < 0) {
        return -1;
    }

    uint32_t sstatus = sum_enter();
    memcpy(udst, ksrc, n);
    sum_leave(sstatus);
    return 0;
}