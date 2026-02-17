#include "syscall_internal.h"
#include "kernel/kernel.h"
#include "net/net.h"

void syscall_handle_ping_tx(struct trap_frame *f) {
    uint32_t dst_ip = (uint32_t) f->a0;
    uint16_t id = (uint16_t) f->a1;
    uint16_t seq = (uint16_t) f->a2;

    int ret = net_ping_send_once(dst_ip, id, seq);
    f->a0 = ret;
}
