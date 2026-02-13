#include "commands_sys.h"
#include "commonlibs.h"
#include "user_syscall.h"
#include "shell.h"

void shell_cmd_kernel_info(void) {
    struct kernel_info info;
    int ret = kernel_info(&info);
    if (ret < 0) {
        printf("kernel_info failed\n");
    } else {
        printf("version       : %s\n", info.version);
        printf("total pages   : %d\n", info.total_pages);
        printf("page size     : %d bytes\n", info.page_size);
        printf("kernel base   : 0x%x\n", info.kernel_base);
        printf("user base     : 0x%x\n", info.user_base);
        printf("proc max      : %d\n", info.proc_max);
        printf("kernel stack  : %d bytes/proc\n", info.kernel_stack_bytes);
        printf("time slice    : %d ticks\n", info.time_slice_ticks);
        printf("timer interval: %d ms\n", info.timer_interval_ms);
        printf("ramfs node max: %d\n", info.ramfs_node_max);
        printf("ramfs size max: %d\n", info.ramfs_size_max);
    }
}

void shell_cmd_history(void) {
    history_print();
}

void shell_cmd_bitmap(void) {
    int total = 0;
    int used = 0;
    for (int i = 0;; i++) {
        int bit = bitmap(i);
        if (bit < 0) {
            break;
        }
        if (bit == 1) used++;
        total++;
    }
    printf("bitmap: total=%d/used=%d/free=%d\n", total, used, (total - used));
}

__attribute__((noreturn))
void shell_cmd_shutdown(void) {
    exit();
}
