#include "stdtypes.h"
#include "commonlibs.h"
#include "kernel.h"
#include "memory.h"
#include "process.h"


extern char __kernel_base[], __free_ram_end[];


struct process procs[PROCS_MAX];
struct process *current_proc;
struct process *idle_proc;


static void free_process_memory(struct process *proc) {
    if (!proc || !proc->page_table) {
        return;
    }

    uint32_t *table1 = proc->page_table;

    // Free mapped user pages.
    for (uint32_t i = 0; i < proc->user_pages; i++) {
        uint32_t vaddr = USER_BASE + i * PAGE_SIZE;
        uint32_t vpn1 = (vaddr >> 22) & 0x3ff;
        uint32_t vpn0 = (vaddr >> 12) & 0x3ff;

        if ((table1[vpn1] & PAGE_V) == 0) {
            continue;
        }

        uint32_t *table0 = (uint32_t *) ((table1[vpn1] >> 10) * PAGE_SIZE);
        if ((table0[vpn0] & PAGE_V) == 0) {
            continue;
        }

        paddr_t paddr = (paddr_t) ((table0[vpn0] >> 10) * PAGE_SIZE);
        free_pages(paddr, 1);
        table0[vpn0] = 0;
    }

    // Free second-level page tables owned by this process.
    for (int vpn1 = 0; vpn1 < 1024; vpn1++) {
        if ((table1[vpn1] & PAGE_V) == 0) {
            continue;
        }

        paddr_t table0_paddr = (paddr_t) ((table1[vpn1] >> 10) * PAGE_SIZE);
        free_pages(table0_paddr, 1);
        table1[vpn1] = 0;
    }

    // Free top-level page table.
    free_pages((paddr_t) table1, 1);

    proc->page_table = NULL;
    proc->user_pages = 0;
}


static void reap_exited_processes(void) {
    for (int i = 0; i < PROCS_MAX; i++) {
        if (procs[i].state == PROC_EXITED) {
            free_process_memory(&procs[i]);
            procs[i].state = PROC_UNUSED;
            procs[i].wait_reason = PROC_WAIT_NONE;
            procs[i].pid = 0;
            procs[i].sp = 0;
        }
    }
}


__attribute__((naked))
void switch_context(uint32_t *prev_sp, uint32_t *next_sp) {
    __asm__ __volatile__(
        // store registry to current process's stack
        "addi sp, sp, -13 * 4\n"
        "sw ra,  0  * 4(sp)\n"
        "sw s0,  1  * 4(sp)\n"
        "sw s1,  2  * 4(sp)\n"
        "sw s2,  3  * 4(sp)\n"
        "sw s3,  4  * 4(sp)\n"
        "sw s4,  5  * 4(sp)\n"
        "sw s5,  6  * 4(sp)\n"
        "sw s6,  7  * 4(sp)\n"
        "sw s7,  8  * 4(sp)\n"
        "sw s8,  9  * 4(sp)\n"
        "sw s9,  10 * 4(sp)\n"
        "sw s10, 11 * 4(sp)\n"
        "sw s11, 12 * 4(sp)\n"

        // store sstatus
        "csrr t0, sstatus\n"
        "addi sp, sp, -4\n"
        "sw t0, 0(sp)\n"

        // change stack pointer
        "sw sp, (a0)\n"
        "lw sp, (a1)\n"

        // restore sstatus
        "lw t0, 0(sp)\n"
        "addi sp, sp, 4\n"
        "csrw sstatus, t0\n"

        // restore registry from next process's stack
        "lw ra,  0  * 4(sp)\n"
        "lw s0,  1  * 4(sp)\n"
        "lw s1,  2  * 4(sp)\n"
        "lw s2,  3  * 4(sp)\n"
        "lw s3,  4  * 4(sp)\n"
        "lw s4,  5  * 4(sp)\n"
        "lw s5,  6  * 4(sp)\n"
        "lw s6,  7  * 4(sp)\n"
        "lw s7,  8  * 4(sp)\n"
        "lw s8,  9  * 4(sp)\n"
        "lw s9,  10 * 4(sp)\n"
        "lw s10, 11 * 4(sp)\n"
        "lw s11, 12 * 4(sp)\n"
        "addi sp, sp, 13 * 4\n"
        "ret\n"
    );
}


struct process *create_process(const void *image, size_t image_size) {
    struct process *proc = NULL;
    int i;

    reap_exited_processes();
    for (i = 0; i < PROCS_MAX; i++) {
        if (procs[i].state == PROC_UNUSED) {
            proc = &procs[i];
            break;
        }
    }

    if (!proc) {
        PANIC("No free process slots");
    }

    uint32_t *sp = (uint32_t *) &proc->stack[sizeof(proc->stack)];
    *--sp = 0;                          // s11
    *--sp = 0;                          // s10
    *--sp = 0;                          // s9
    *--sp = 0;                          // s8
    *--sp = 0;                          // s7
    *--sp = 0;                          // s6
    *--sp = 0;                          // s5
    *--sp = 0;                          // s4
    *--sp = 0;                          // s3
    *--sp = 0;                          // s2
    *--sp = 0;                          // s1
    *--sp = 0;                          // s0
    *--sp = (uint32_t) user_entry;      // ra
    *--sp = (1 << 1);                   // sstatus (sie flag)

    uint32_t *page_table = (uint32_t *) alloc_pages(1);
    for (paddr_t paddr = (paddr_t) __kernel_base;
         paddr < (paddr_t) __free_ram_end;
         paddr += PAGE_SIZE) {
        map_page(page_table, paddr, paddr, PAGE_R | PAGE_W | PAGE_X);
    }

    uint32_t user_pages = (image_size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint32_t off = 0; off < image_size; off += PAGE_SIZE) {
        paddr_t page = alloc_pages(1);

        size_t remaining = image_size - off;
        size_t copy_size = PAGE_SIZE <= remaining ? PAGE_SIZE : remaining;

        memcpy((void *) page, image + off, copy_size);

        map_page(page_table, USER_BASE + off, page,
                 PAGE_U | PAGE_R | PAGE_W | PAGE_X);
    }

    proc->pid = i + 1;
    proc->state = PROC_RUNNABLE;
    proc->wait_reason = PROC_WAIT_NONE;
    proc->user_pages = user_pages;
    proc->sp = (uint32_t) sp;
    proc->page_table = page_table;
    return proc;
}


void yield(void) {
    reap_exited_processes();

    while (1) {
        struct process *next = NULL;
        for (int i = 0; i < PROCS_MAX; i++) {
            struct process *proc = &procs[(current_proc->pid + i) % PROCS_MAX];
            if (proc->state == PROC_RUNNABLE && proc->pid > 0) {
                next = proc;
                break;
            }
        }

        if (!next) {
            uint32_t sstatus = READ_CSR(sstatus);

            // Trap entry uses sscratch/sp swap, so keep sscratch in sync while idling in S-mode.
            __asm__ __volatile__("csrw sscratch, sp");

            WRITE_CSR(sstatus, sstatus | (1 << 1));
            __asm__ __volatile__("wfi");
            WRITE_CSR(sstatus, sstatus);
            continue;
        }

        if (next == current_proc) {
            return;
        }

        __asm__ __volatile__(
            "sfence.vma\n"
            "csrw satp, %[satp]\n"
            "sfence.vma\n"
            "csrw sscratch, %[sscratch]\n"
            :
            : [satp] "r" (SATP_SV32 | ((uint32_t) next->page_table / PAGE_SIZE)),
              [sscratch] "r" ((uint32_t) &next->stack[sizeof(next->stack)])
        );

        struct process *prev = current_proc;
        current_proc = next;
        switch_context(&prev->sp, &next->sp);
        return;
    }
}


void wakeup_input_waiters(void) {
    for (int i = 0; i < PROCS_MAX; i++) {
        if (procs[i].state == PROC_WAITTING &&
            procs[i].wait_reason == PROC_WAIT_CONSOLE_INPUT) {
            procs[i].state = PROC_RUNNABLE;
            procs[i].wait_reason = PROC_WAIT_NONE;
        }
    }
}
