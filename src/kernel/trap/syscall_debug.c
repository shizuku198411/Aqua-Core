#include "syscall_internal.h"
#include "mm/memory.h"
#include "kernel/kernel.h"
#include "kernel/page_access.h"

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

    uint32_t sstatus = sum_enter();
    *user_info = info;
    sum_leave(sstatus);

    f->a0 = 0;
}
