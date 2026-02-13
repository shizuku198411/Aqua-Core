#include "kernel.h"
#include "version.h"
#include "timer.h"
#include "stdtypes.h"
#include "commonlibs.h"
#include "memory.h"
#include "process.h"
#include "sbi.h"
#include "fs.h"
#include "fs_internal.h"


extern char __bss[], __bss_end[], __stack_top[];
extern char _binary___bin_shell_bin_start[], _binary___bin_shell_bin_size[];
extern struct process *current_proc;
extern struct process *idle_proc;
extern struct process *init_proc;

static uint32_t kernel_total_pages;

__attribute__((naked))
__attribute__((aligned(4)))
void kernel_entry(void) {
    __asm__ __volatile__(
        // Swap sp/sscratch only when trapped from U-mode (SPP=0).
        // For S-mode traps, keep the current kernel stack pointer.
        "csrr t0, sstatus\n"
        "andi t0, t0, 0x100\n"
        "bnez t0, 1f\n"
        "csrrw sp, sscratch, sp\n"
        "j 2f\n"
        "1:\n"
        "csrw sscratch, sp\n"
        "2:\n"

        // Always run trap handler with interrupts disabled to avoid nested
        // timer traps corrupting the current kernel stack/proc fields.
        "csrc sstatus, 2\n"

        "addi sp, sp, -4 * 31\n"
        "sw ra,  4 * 0(sp)\n"
        "sw gp,  4 * 1(sp)\n"
        "sw tp,  4 * 2(sp)\n"
        "sw t0,  4 * 3(sp)\n"
        "sw t1,  4 * 4(sp)\n"
        "sw t2,  4 * 5(sp)\n"
        "sw t3,  4 * 6(sp)\n"
        "sw t4,  4 * 7(sp)\n"
        "sw t5,  4 * 8(sp)\n"
        "sw t6,  4 * 9(sp)\n"
        "sw a0,  4 * 10(sp)\n"
        "sw a1,  4 * 11(sp)\n"
        "sw a2,  4 * 12(sp)\n"
        "sw a3,  4 * 13(sp)\n"
        "sw a4,  4 * 14(sp)\n"
        "sw a5,  4 * 15(sp)\n"
        "sw a6,  4 * 16(sp)\n"
        "sw a7,  4 * 17(sp)\n"
        "sw s0,  4 * 18(sp)\n"
        "sw s1,  4 * 19(sp)\n"
        "sw s2,  4 * 20(sp)\n"
        "sw s3,  4 * 21(sp)\n"
        "sw s4,  4 * 22(sp)\n"
        "sw s5,  4 * 23(sp)\n"
        "sw s6,  4 * 24(sp)\n"
        "sw s7,  4 * 25(sp)\n"
        "sw s8,  4 * 26(sp)\n"
        "sw s9,  4 * 27(sp)\n"
        "sw s10, 4 * 28(sp)\n"
        "sw s11, 4 * 29(sp)\n"

        "csrr a0, sscratch\n"
        "sw a0, 4 * 30(sp)\n"

        "addi a0, sp, 4 * 31\n"
        "csrw sscratch, a0\n"

        "mv a0, sp\n"
        "call handle_trap\n"

        "lw ra,  4 * 0(sp)\n"
        "lw gp,  4 * 1(sp)\n"
        "lw tp,  4 * 2(sp)\n"
        "lw t0,  4 * 3(sp)\n"
        "lw t1,  4 * 4(sp)\n"
        "lw t2,  4 * 5(sp)\n"
        "lw t3,  4 * 6(sp)\n"
        "lw t4,  4 * 7(sp)\n"
        "lw t5,  4 * 8(sp)\n"
        "lw t6,  4 * 9(sp)\n"
        "lw a0,  4 * 10(sp)\n"
        "lw a1,  4 * 11(sp)\n"
        "lw a2,  4 * 12(sp)\n"
        "lw a3,  4 * 13(sp)\n"
        "lw a4,  4 * 14(sp)\n"
        "lw a5,  4 * 15(sp)\n"
        "lw a6,  4 * 16(sp)\n"
        "lw a7,  4 * 17(sp)\n"
        "lw s0,  4 * 18(sp)\n"
        "lw s1,  4 * 19(sp)\n"
        "lw s2,  4 * 20(sp)\n"
        "lw s3,  4 * 21(sp)\n"
        "lw s4,  4 * 22(sp)\n"
        "lw s5,  4 * 23(sp)\n"
        "lw s6,  4 * 24(sp)\n"
        "lw s7,  4 * 25(sp)\n"
        "lw s8,  4 * 26(sp)\n"
        "lw s9,  4 * 27(sp)\n"
        "lw s10, 4 * 28(sp)\n"
        "lw s11, 4 * 29(sp)\n"
        "lw sp,  4 * 30(sp)\n"
        "sret\n"
    );
}


void user_entry(void) {
    __asm__ __volatile__ (
        "csrw sepc, %[sepc]\n"
        "csrw sstatus, %[sstatus]\n"
        "sret\n"
        :
        : [sepc] "r" (USER_BASE),
          [sstatus] "r" (SSTATUS_SPIE)
    );
}


__attribute__((noreturn))
void kernel_shutdown(void) {
    printf("[*] kernel shutdown requested.\n");
    sbi_shutdown();
    __builtin_unreachable();
}


void banner(void) {
    printf("\n\n");
    printf("\033[34m");
    printf("                /\\     \n");
    printf("               /  \\    \n");
    printf("              / \033[0m/\\ \033[34m\\   \n");
    printf("              \\_\033[0m\\/\033[34m_/   \n\n");
    printf("\033[0m");
    printf("    Welcome to \033[34mAquaCore\033[0m %s\n\n", KERNEL_VERSION);
}


void kernel_bootstrap(void) {
    printf("[*] kernel bootstrap started.\n");

    // init memory
    printf("[*] initialize memory...");
    memset(__bss, 0, (size_t) __bss_end - (size_t) __bss);
    kernel_total_pages = memory_init();
    printf("done.\n");

    // set trap handler to stvec registry
    printf("[*] set trapvector to reg:stvec...");
    WRITE_CSR(stvec, (uint32_t) kernel_entry);
    printf("done.\n");

    // Ensure trap-entry stack swap source is valid before first timer interrupt.
    uint32_t kernel_sp;
    __asm__ __volatile__("mv %0, sp" : "=r"(kernel_sp));
    WRITE_CSR(sscratch, kernel_sp);

    // enable supervisor timer interrupt
    printf("[*] enable timer interrupt...");
    enable_timer_interrupt();
    timer_set_next();
    printf("done.\n");

    // initialize in-memory filesystem
    printf("[*] initialize filesystem...");
    fs_init();
    printf("    done.\n");

    // create idle process
    printf("[*] setup process...");
    idle_proc = create_process(NULL, 0, "idle");
    idle_proc->pid = 0;
    current_proc = idle_proc;
    printf("done.\n");

    // print kernel info
    printf("[*] kernel information:\n");
    printf("     version       : %s\n", KERNEL_VERSION);
    printf("     total pages   : %d\n", kernel_total_pages);
    printf("     page size     : %d bytes\n", PAGE_SIZE);
    printf("     kernel base   : 0x%x\n", KERNEL_BASE);
    printf("     user base     : 0x%x\n", USER_BASE);
    printf("     proc max      : %d\n", PROCS_MAX);
    printf("     kernel stack  : %d bytes/proc\n", (int) sizeof(procs[0].stack));
    printf("     time slice    : %d ticks\n", SCHED_TIME_SLICE_TICKS);
    printf("     timer interval: %d ms\n", (TIMER_INTERVAL / 10000));
    printf("     ramfs node max: %d\n", FS_MAX_NODES);
    printf("     ramfs size max: %d\n", FS_FILE_MAX_SIZE);

    printf("[*] kernel bootstrap completed.\n");
}

void kernel_get_info(struct kernel_info *out) {
    if (!out) {
        return;
    }

    for (int i = 0; i < KERNEL_VERSION_MAX; i++) {
        out->version[i] = '\0';
    }
    for (int i = 0; i < KERNEL_VERSION_MAX - 1 && KERNEL_VERSION[i] != '\0'; i++) {
        out->version[i] = KERNEL_VERSION[i];
    }

    out->total_pages = kernel_total_pages;
    out->page_size = PAGE_SIZE;
    out->kernel_base = KERNEL_BASE;
    out->user_base = USER_BASE;
    out->proc_max = PROCS_MAX;
    out->kernel_stack_bytes = (uint32_t) sizeof(((struct process *) 0)->stack);
    out->time_slice_ticks = SCHED_TIME_SLICE_TICKS;
    out->timer_interval_ms = TIMER_INTERVAL / 10000;
    out->ramfs_node_max = FS_MAX_NODES;
    out->ramfs_size_max = FS_FILE_MAX_SIZE;
}

void kernel_main(void) {
    kernel_bootstrap();
    banner();

    // start shell (init)
    init_proc = create_process(_binary___bin_shell_bin_start,
                               (size_t) _binary___bin_shell_bin_size,
                               "shell");
    yield();

    __builtin_unreachable();
}


__attribute__((section(".text.boot")))
__attribute__((naked))
void boot(void) {
    __asm__ __volatile__(
        "mv sp, %[stack_top]\n"
        "csrw sscratch, sp\n"       // init sscratch before interrupt
        "j kernel_main\n"
        :
        : [stack_top] "r" (__stack_top)
    );
}
