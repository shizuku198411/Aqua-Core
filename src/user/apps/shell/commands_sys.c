#include "commands_sys.h"
#include "commonlibs.h"
#include "user_syscall.h"
#include "rtc.h"
#include "shell.h"

void shell_cmd_kernel_info(void) {
    struct kernel_info info;
    int ret = kernel_info(&info);
    if (ret < 0) {
        shell_printf("kernel_info failed\n");
    } else {
        shell_printf("version       : %s\n", info.version);
        shell_printf("kernel time   : %s\n", info.time);
        shell_printf("total pages   : %d\n", info.total_pages);
        shell_printf("page size     : %d bytes\n", info.page_size);
        shell_printf("kernel base   : 0x%x\n", info.kernel_base);
        shell_printf("user base     : 0x%x\n", info.user_base);
        shell_printf("proc max      : %d\n", info.proc_max);
        shell_printf("kernel stack  : %d bytes/proc\n", info.kernel_stack_bytes);
        shell_printf("time slice    : %d ticks\n", info.time_slice_ticks);
        shell_printf("timer interval: %d ms\n", info.timer_interval_ms);
        shell_printf("ramfs node max: %d\n", info.ramfs_node_max);
        shell_printf("ramfs size max: %d bytes\n", info.ramfs_size_max);
        shell_printf("pfs blk count : %d\n", info.pfs_block_count);
        shell_printf("pfs blk size  : %d bytes\n", info.pfs_block_size);
        shell_printf("pfs img blks  : %d\n", info.pfs_image_blocks);
        shell_printf("pfs img bytes : %d\n", info.pfs_image_bytes);
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
    shell_printf("bitmap: total=%d/used=%d/free=%d\n", total, used, (total - used));
}

void shell_cmd_gettime(void) {
    struct time_spec info;
    int ret = gettime(&info);
    if (ret < 0) {
        return;
    }

    uint64_t sec = ((uint64_t) info.sec_hi << 32) | info.sec_lo;
    char ts[40];
    unix_time_to_utc_str(sec, ts, sizeof(ts));
    shell_printf("%s\n", ts);
}

void shell_cmd_stdin_test(void) {
    char buf[64];
    int total = 0;

    while (1) {
        int n = shell_read_input(buf, sizeof(buf) - 1);
        if (n <= 0) {
            break;
        }
        total += n;
        buf[n] = '\0';
        shell_printf("%s", buf);
    }
    shell_printf("\n[stdin_test] bytes=%d\n", total);
}

__attribute__((noreturn))
void shell_cmd_shutdown(void) {
    exit();
}
