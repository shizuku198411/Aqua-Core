#include "user_syscall.h"
#include "commonlibs.h"

int main(int argc, char **argv) {
    (void) argc;
    (void) argv;

    struct kernel_info info;
    int ret = kernel_info(&info);
    if (ret < 0) {
        printf("kernel_info failed\n");
        return -1;
    }

    printf("version       : %s\n", info.version);
    printf("kernel time   : %s\n", info.time);
    printf("total pages   : %d\n", info.total_pages);
    printf("page size     : %d bytes\n", info.page_size);
    printf("kernel base   : 0x%x\n", info.kernel_base);
    printf("user base     : 0x%x\n", info.user_base);
    printf("proc max      : %d\n", info.proc_max);
    printf("kernel stack  : %d bytes/proc\n", info.kernel_stack_bytes);
    printf("time slice    : %d ticks\n", info.time_slice_ticks);
    printf("timer interval: %d ms\n", info.timer_interval_ms);
    printf("ramfs node max: %d\n", info.ramfs_node_max);
    printf("ramfs size max: %d bytes\n", info.ramfs_size_max);
    printf("pfs blk count : %d\n", info.pfs_block_count);
    printf("pfs blk size  : %d bytes\n", info.pfs_block_size);
    printf("pfs img blks  : %d\n", info.pfs_image_blocks);
    printf("pfs img bytes : %d\n", info.pfs_image_bytes);
    return 0;
}
