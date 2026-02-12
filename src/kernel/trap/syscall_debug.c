#include "syscall_internal.h"
#include "memory.h"

void syscall_handle_bitmap(struct trap_frame *f) {
    f->a0 = bitmap_page_state((int) f->a0);
}
