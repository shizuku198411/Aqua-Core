#include "syscall_internal.h"
#include "memory.h"
#include "kernel.h"

#define SSTATUS_SUM (1u << 18)

void syscall_handle_bitmap(struct trap_frame *f) {
    f->a0 = bitmap_page_state((int) f->a0);
}

void syscall_handle_kernel_info(struct trap_frame *f) {
    struct kernel_info *user_info = (struct kernel_info *) f->a0;
    if (!user_info) {
        f->a0 = -1;
        return;
    }

    struct kernel_info info;
    kernel_get_info(&info);

    uint32_t sstatus = READ_CSR(sstatus);
    WRITE_CSR(sstatus, sstatus | SSTATUS_SUM);
    *user_info = info;
    WRITE_CSR(sstatus, sstatus);

    f->a0 = 0;
}
