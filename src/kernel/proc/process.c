#include "stdtypes.h"
#include "commonlibs.h"
#include "kernel.h"
#include "memory.h"
#include "process.h"
#include "fs_internal.h"
#include "rtc.h"


extern char __kernel_base[], __free_ram_end[];


struct process procs[PROCS_MAX];
struct process *current_proc;
struct process *idle_proc;
struct process *init_proc;

static bool need_resched;

static void set_process_name(struct process *proc, const char *name) {
    for (int i = 0; i < PROC_NAME_MAX; i++) {
        proc->name[i] = '\0';
    }

    if (!name) {
        return;
    }

    for (int i = 0; i < PROC_NAME_MAX - 1 && name[i] != '\0'; i++) {
        proc->name[i] = name[i];
    }
}

static void clear_exec_args(struct process *proc) {
    if (!proc) {
        return;
    }

    proc->exec_argc = 0;
    for (int i = 0; i < PROC_EXEC_ARGV_MAX; i++) {
        for (int j = 0; j < PROC_EXEC_ARG_LEN; j++) {
            proc->exec_argv[i][j] = '\0';
        }
    }
}

static void set_exec_args(struct process *proc,
                          int argc,
                          const char argv[PROC_EXEC_ARGV_MAX][PROC_EXEC_ARG_LEN]) {
    clear_exec_args(proc);
    if (!proc || !argv || argc <= 0) {
        return;
    }

    int n = argc;
    if (n > PROC_EXEC_ARGV_MAX) {
        n = PROC_EXEC_ARGV_MAX;
    }
    proc->exec_argc = n;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < PROC_EXEC_ARG_LEN - 1; j++) {
            proc->exec_argv[i][j] = argv[i][j];
            if (argv[i][j] == '\0') {
                break;
            }
        }
        proc->exec_argv[i][PROC_EXEC_ARG_LEN - 1] = '\0';
    }
}


static struct process *find_process_by_pid(int pid) {
    if (pid <= 0) {
        return NULL;
    }

    for (int i = 0; i < PROCS_MAX; i++) {
        if (procs[i].pid == pid && procs[i].state != PROC_UNUSED) {
            return &procs[i];
        }
    }

    return NULL;
}


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


static void recycle_process_slot(struct process *proc) {
    if (!proc) {
        return;
    }

    fs_on_process_recycle(proc->pid);
    free_process_memory(proc);
    proc->state = PROC_UNUSED;
    set_process_name(proc, NULL);
    proc->wait_reason = PROC_WAIT_NONE;
    proc->wait_pid = -1;
    proc->parent_pid = 0;
    proc->pid = 0;
    proc->sp = 0;
    proc->time_slice = SCHED_TIME_SLICE_TICKS;
    proc->run_ticks = 0;
    proc->schedule_count = 0;
    proc->ipc_has_message = 0;
    proc->ipc_from_pid = 0;
    proc->ipc_message = 0;
    clear_exec_args(proc);
}


static void reap_exited_processes(void) {
    for (int i = 0; i < PROCS_MAX; i++) {
        if (procs[i].state != PROC_EXITED) {
            continue;
        }

        // Processes with no parent can be reclaimed immediately.
        // Parented processes are kept as zombies until waitpid() collects them.
        if (procs[i].parent_pid == 0) {
            recycle_process_slot(&procs[i]);
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


struct process *create_process(const void *image, size_t image_size, const char *name) {
    struct process *proc = NULL;
    int i;

    reap_exited_processes();
    for (i = 0; i < PROCS_MAX; i++) {
        if (procs[i].state == PROC_UNUSED) {
            proc = &procs[i];
            break;
        }
    }
    // no free slot
    if (!proc) {
        return NULL;
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
    // Keep interrupts disabled while running kernel context after first schedule-in.
    // user_entry() sets the user-visible sstatus before sret.
    *--sp = 0;                          // sstatus

    uint32_t *page_table = (uint32_t *) alloc_pages(1);
    for (paddr_t paddr = (paddr_t) __kernel_base;
         paddr < (paddr_t) __free_ram_end;
         paddr += PAGE_SIZE) {
        map_page(page_table, paddr, paddr, PAGE_R | PAGE_W | PAGE_X);
    }

    // map MMIO region for device access while running in process page table.
    for (paddr_t paddr = MMIO_BASE; paddr < MMIO_END; paddr += PAGE_SIZE) {
        map_page(page_table, paddr, paddr, PAGE_R | PAGE_W);
    }
    // map RTC MMIO
    for (paddr_t paddr = RTC_MMIO_BASE; paddr < RTC_MMIO_END; paddr += PAGE_SIZE) {
        map_page(page_table, paddr, paddr, PAGE_R | PAGE_W);
    }
    // map user page
    uint32_t user_pages = (image_size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint32_t off = 0; off < image_size; off += PAGE_SIZE) {
        paddr_t page = alloc_pages(1);

        size_t remaining = image_size - off;
        size_t copy_size = PAGE_SIZE <= remaining ? PAGE_SIZE : remaining;

        memcpy((void *) page, image + off, copy_size);

        map_page(page_table, USER_BASE + off, page,
                 PAGE_U | PAGE_R | PAGE_W | PAGE_X);
    }

    // get root mount/node index
    int root_mount_idx, root_node_idx;
    if (fs_get_root_entry(&root_mount_idx, &root_node_idx) < 0) {
        recycle_process_slot(proc);
        return NULL;
    }

    proc->pid = i;
    proc->state = PROC_RUNNABLE;
    set_process_name(proc, name);
    proc->wait_reason = PROC_WAIT_NONE;
    proc->wait_pid = -1;
    proc->parent_pid = 0;
    proc->user_pages = user_pages;
    proc->sp = (uint32_t) sp;
    proc->page_table = page_table;
    proc->time_slice = SCHED_TIME_SLICE_TICKS;
    proc->run_ticks = 0;
    proc->schedule_count = 0;
    proc->ipc_has_message = 0;
    proc->ipc_from_pid = 0;
    proc->ipc_message = 0;
    clear_exec_args(proc);
    proc->root_mount_idx = root_mount_idx;
    proc->root_node_idx = root_node_idx;
    strcpy_s(proc->root_path, FS_PATH_MAX, "/");
    proc->cwd_mount_idx = root_mount_idx;
    proc->cwd_node_idx = root_node_idx;
    strcpy_s(proc->cwd_path, FS_PATH_MAX, "/");
    return proc;
}


__attribute__((naked))
static void fork_child_trap_return(void) {
    __asm__ __volatile__(
        // s11: child resume sepc (= parent sepc + 4)
        // s0 : struct trap_frame *child_tf
        "csrw sepc, s11\n"
        "mv gp, s0\n"

        "lw ra,  4 * 0(gp)\n"
        "lw tp,  4 * 2(gp)\n"
        "lw t0,  4 * 3(gp)\n"
        "lw t1,  4 * 4(gp)\n"
        "lw t2,  4 * 5(gp)\n"
        "lw t3,  4 * 6(gp)\n"
        "lw t4,  4 * 7(gp)\n"
        "lw t5,  4 * 8(gp)\n"
        "lw t6,  4 * 9(gp)\n"
        "lw a0,  4 * 10(gp)\n"
        "lw a1,  4 * 11(gp)\n"
        "lw a2,  4 * 12(gp)\n"
        "lw a3,  4 * 13(gp)\n"
        "lw a4,  4 * 14(gp)\n"
        "lw a5,  4 * 15(gp)\n"
        "lw a6,  4 * 16(gp)\n"
        "lw a7,  4 * 17(gp)\n"
        "lw s0,  4 * 18(gp)\n"
        "lw s1,  4 * 19(gp)\n"
        "lw s2,  4 * 20(gp)\n"
        "lw s3,  4 * 21(gp)\n"
        "lw s4,  4 * 22(gp)\n"
        "lw s5,  4 * 23(gp)\n"
        "lw s6,  4 * 24(gp)\n"
        "lw s7,  4 * 25(gp)\n"
        "lw s8,  4 * 26(gp)\n"
        "lw s9,  4 * 27(gp)\n"
        "lw s10, 4 * 28(gp)\n"
        "lw s11, 4 * 29(gp)\n"
        "lw sp,  4 * 30(gp)\n"
        "lw gp,  4 * 1(gp)\n"
        "sret\n"
    );
}

struct process *alloc_proc_slot() {
    struct process *proc = NULL;
    reap_exited_processes();
    int i;
    for (i = 0; i < PROCS_MAX; i++) {
        if (procs[i].state == PROC_UNUSED) {
            proc = &procs[i];
            break;
        }
    }
    if (!proc) {
        return NULL;
    }

    // initialize process
    proc->pid = i;
    proc->state = PROC_UNUSED;
    proc->wait_reason = PROC_WAIT_NONE;
    proc->wait_pid = -1;
    proc->parent_pid = 0;
    clear_exec_args(proc);
    return proc;
}

int process_fork(struct trap_frame *parent_tf) {
    struct process *child = alloc_proc_slot();
    if (!child || !parent_tf || !current_proc) goto fail;
    if (current_proc->state != PROC_RUNNABLE && current_proc->state != PROC_WAITTING) goto fail;

    child->parent_pid = current_proc->pid;
    strcpy_s(child->name, PROC_NAME_MAX, current_proc->name);
    child->ipc_has_message = 0;
    child->ipc_from_pid = 0;
    child->ipc_message = 0;
    child->exec_argc = current_proc->exec_argc;
    for (int i = 0; i < PROC_EXEC_ARGV_MAX; i++) {
        for (int j = 0; j < PROC_EXEC_ARG_LEN; j++) {
            child->exec_argv[i][j] = current_proc->exec_argv[i][j];
        }
    }
    child->root_mount_idx = current_proc->root_mount_idx;
    child->root_node_idx = current_proc->root_node_idx;
    strcpy_s(child->root_path, FS_PATH_MAX, current_proc->root_path);
    child->cwd_mount_idx = current_proc->cwd_mount_idx;
    child->cwd_node_idx = current_proc->cwd_node_idx;
    strcpy_s(child->cwd_path, FS_PATH_MAX, current_proc->cwd_path);

    // child page table + user pages copy
    uint32_t *page_table = (uint32_t *)alloc_pages(1);
    if (!page_table) goto fail;

    for (paddr_t p = (paddr_t)__kernel_base; p < (paddr_t)__free_ram_end; p += PAGE_SIZE)
        map_page(page_table, p, p, PAGE_R | PAGE_W | PAGE_X);
    for (paddr_t p = MMIO_BASE; p < MMIO_END; p += PAGE_SIZE)
        map_page(page_table, p, p, PAGE_R | PAGE_W);
    for (paddr_t p = RTC_MMIO_BASE; p < RTC_MMIO_END; p += PAGE_SIZE)
        map_page(page_table, p, p, PAGE_R | PAGE_W);

    child->page_table = page_table;
    child->user_pages = current_proc->user_pages;

    for (uint32_t i = 0; i < child->user_pages; i++) {
        uint32_t vaddr = USER_BASE + i * PAGE_SIZE;
        uint32_t vpn1 = (vaddr >> 22) & 0x3ff;
        uint32_t vpn0 = (vaddr >> 12) & 0x3ff;

        uint32_t *pt1 = current_proc->page_table;
        if ((pt1[vpn1] & PAGE_V) == 0) goto fail;
        uint32_t *pt0 = (uint32_t *)((pt1[vpn1] >> 10) * PAGE_SIZE);
        if ((pt0[vpn0] & PAGE_V) == 0) goto fail;

        paddr_t parent_page = (paddr_t)((pt0[vpn0] >> 10) * PAGE_SIZE);
        paddr_t child_page = alloc_pages(1);
        if (!child_page) goto fail;

        memcpy((void *)child_page, (const void *)parent_page, PAGE_SIZE);

        uint32_t flags = (pt0[vpn0] & 0x3ff) & ~PAGE_V;
        map_page(child->page_table, vaddr, child_page, flags);
    }

    // child trap-return context build
    uint8_t *kstack_top = &child->stack[sizeof(child->stack)];

    struct trap_frame *child_tf = (struct trap_frame *)(kstack_top - sizeof(struct trap_frame));
    *child_tf = *parent_tf;
    child_tf->a0 = 0; // child return value

    // switch_context frame: [sstatus][ra][s0..s11]
    uint32_t *ksp = (uint32_t *)((uint8_t *)child_tf - (14 * sizeof(uint32_t)));
    ksp[0]  = READ_CSR(sstatus);                    // saved sstatus
    ksp[1]  = (uint32_t)fork_child_trap_return;     // ra
    ksp[2]  = (uint32_t)child_tf;                   // s0 = trap frame ptr
    ksp[3]  = 0; ksp[4] = 0; ksp[5] = 0; ksp[6] = 0;
    ksp[7]  = 0; ksp[8] = 0; ksp[9] = 0; ksp[10] = 0;
    ksp[11] = 0; ksp[12] = 0;
    ksp[13] = READ_CSR(sepc) + 4;                   // s11 = child resume pc

    child->sp = (uint32_t)ksp;

    if (fs_fork_copy_fds(current_proc->pid, child->pid) < 0) {
        goto fail;
    }

    // finalize
    child->state = PROC_RUNNABLE;
    child->time_slice = SCHED_TIME_SLICE_TICKS;
    child->run_ticks = 0;
    child->schedule_count = 0;

    return child->pid;

fail:
    recycle_process_slot(child);
    return -1;
}

static void free_user_pages_only(struct process *proc) {
    if (!proc || !proc->page_table) return;

    uint32_t *table1 = proc->page_table;
    for (uint32_t i = 0; i < proc->user_pages; i++) {
        uint32_t vaddr = (USER_BASE + i * PAGE_SIZE);
        uint32_t vpn1 = (vaddr >> 22) &0x3ff;
        uint32_t vpn0 = (vaddr >> 12) &0x3ff;

        if ((table1[vpn1] & PAGE_V) == 0) continue;
        uint32_t *table0 = (uint32_t *)((table1[vpn1] >> 10) * PAGE_SIZE);
        if ((table0[vpn0] & PAGE_V) == 0) continue;

        paddr_t paddr = (paddr_t)((table0[vpn0] >> 10) * PAGE_SIZE);
        free_pages(paddr, 1);
        table0[vpn0] = 0;
    }
    proc->user_pages = 0;
}

int process_exec(const void *image,
                 size_t image_size,
                 const char *name,
                 int argc,
                 const char argv[PROC_EXEC_ARGV_MAX][PROC_EXEC_ARG_LEN]) {
    if (!current_proc || !image || image_size == 0) {
        return -1;
    }

    // free old user page
    free_user_pages_only(current_proc);

    // map new image
    uint32_t pages = (uint32_t)((image_size + PAGE_SIZE - 1) / PAGE_SIZE);
    for (uint32_t off = 0; off < image_size; off += PAGE_SIZE) {
        paddr_t page = alloc_pages(1);
        if (!page) {
            return -1;
        }
        size_t remain = image_size - off;
        size_t copy_size = remain < PAGE_SIZE ? remain : PAGE_SIZE;
        memcpy((void *)page, (const uint8_t *)image + off, copy_size);

        map_page(current_proc->page_table, USER_BASE + off, page, PAGE_U | PAGE_R | PAGE_W | PAGE_X);
    }

    // update meta
    current_proc->user_pages = pages;
    set_process_name(current_proc, name);
    current_proc->wait_reason = PROC_WAIT_NONE;
    current_proc->wait_pid = -1;
    current_proc->time_slice = SCHED_TIME_SLICE_TICKS;
    current_proc->run_ticks = 0;
    set_exec_args(current_proc, argc, argv);

    WRITE_CSR(sepc, USER_BASE);

    return 0;
}

void scheduler_on_timer_tick(void) {
    if (!current_proc || current_proc->state != PROC_RUNNABLE || current_proc->pid <= 0) {
        return;
    }

    current_proc->run_ticks++;

    if (current_proc->time_slice > 0) {
        current_proc->time_slice--;
    }

    if (current_proc->time_slice == 0) {
        need_resched = true;
    }
}


bool scheduler_should_yield(void) {
    return need_resched;
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

            // Keep sscratch on a stable trap-entry stack pointer for the current process.
            WRITE_CSR(sscratch, (uint32_t) &current_proc->stack[sizeof(current_proc->stack)]);

            WRITE_CSR(sstatus, sstatus | (1 << 1));
            __asm__ __volatile__("wfi");
            WRITE_CSR(sstatus, sstatus);
            continue;
        }

        if (next == current_proc) {
            if (current_proc->time_slice == 0) {
                current_proc->time_slice = SCHED_TIME_SLICE_TICKS;
            }
            WRITE_CSR(sscratch, (uint32_t) &current_proc->stack[sizeof(current_proc->stack)]);
            need_resched = false;
            return;
        }

        next->time_slice = SCHED_TIME_SLICE_TICKS;
        next->schedule_count++;
        need_resched = false;

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


void notify_child_exit(struct process *child) {
    if (!child || child->parent_pid <= 0) {
        return;
    }

    int parent_pid = child->parent_pid;
    for (int i = 0; i < PROCS_MAX; i++) {
        struct process *proc = &procs[i];
        if (proc->pid != parent_pid || proc->state != PROC_WAITTING) {
            continue;
        }
        if (proc->wait_reason != PROC_WAIT_CHILD_EXIT) {
            continue;
        }

        if (proc->wait_pid == -1 || proc->wait_pid == child->pid) {
            proc->state = PROC_RUNNABLE;
            proc->wait_reason = PROC_WAIT_NONE;
            proc->wait_pid = -1;
        }
    }
}


void orphan_children(int parent_pid) {
    if (parent_pid <= 0) {
        return;
    }

    for (int i = 0; i < PROCS_MAX; i++) {
        if (procs[i].parent_pid == parent_pid) {
            procs[i].parent_pid = 0;
        }
    }
}


int wait_for_child_exit(int parent_pid, int target_pid) {
    if (parent_pid <= 0) {
        return -1;
    }

    while (1) {
        bool has_child = false;

        for (int i = 0; i < PROCS_MAX; i++) {
            struct process *proc = &procs[i];
            if (proc->state == PROC_UNUSED || proc->parent_pid != parent_pid) {
                continue;
            }

            if (target_pid != -1 && proc->pid != target_pid) {
                continue;
            }

            has_child = true;
            if (proc->state == PROC_EXITED) {
                int exited_pid = proc->pid;
                recycle_process_slot(proc);
                return exited_pid;
            }
        }

        if (!has_child) {
            return -1;
        }

        current_proc->wait_reason = PROC_WAIT_CHILD_EXIT;
        current_proc->wait_pid = target_pid;
        current_proc->state = PROC_WAITTING;
        yield();
    }
}


int process_ipc_send(int src_pid, int dst_pid, uint32_t message) {
    struct process *dst = find_process_by_pid(dst_pid);
    if (!dst || dst->state == PROC_EXITED) {
        return -1;
    }

    if (dst->ipc_has_message) {
        return -2;
    }

    dst->ipc_has_message = 1;
    dst->ipc_from_pid = src_pid;
    dst->ipc_message = message;

    if (dst->state == PROC_WAITTING && dst->wait_reason == PROC_WAIT_IPC_RECV) {
        dst->state = PROC_RUNNABLE;
        dst->wait_reason = PROC_WAIT_NONE;
    }

    return 0;
}


int process_ipc_recv(int self_pid, int *from_pid, uint32_t *message) {
    struct process *self = find_process_by_pid(self_pid);
    if (!self) {
        return -1;
    }

    while (!self->ipc_has_message) {
        self->wait_reason = PROC_WAIT_IPC_RECV;
        self->wait_pid = -1;
        self->state = PROC_WAITTING;
        yield();
    }

    if (from_pid) {
        *from_pid = self->ipc_from_pid;
    }
    if (message) {
        *message = self->ipc_message;
    }

    self->ipc_has_message = 0;
    self->ipc_from_pid = 0;
    self->ipc_message = 0;
    return 0;
}

int process_kill(int target_pid) {
    if (target_pid <= 0) {
        return -1;
    }

    struct process *target = find_process_by_pid(target_pid);
    if (!target || target->state == PROC_EXITED) {
        return -2;
    }

    if (target == init_proc) {
        return -3;
    }

    int killed_pid = target->pid;
    orphan_children(target->pid);
    target->state = PROC_EXITED;
    target->wait_reason = PROC_WAIT_NONE;
    target->wait_pid = -1;
    notify_child_exit(target);

    if (target == current_proc) {
        // Self-kill cannot free current stack/context immediately.
        target->parent_pid = 0;
        yield();
        PANIC("killed process resumed unexpectedly");
    }

    // kill command semantics: reclaim target slot immediately.
    target->parent_pid = 0;
    recycle_process_slot(target);
    return killed_pid;
}

static const char *proc_state_str(int state) {
    switch (state) {
        case PROC_UNUSED:   return "UNUSED";
        case PROC_RUNNABLE: return "RUN";
        case PROC_WAITTING: return "WAIT";
        case PROC_EXITED:   return "EXIT";
        default:            return "UNKNOWN";
    }
}

static const char *proc_wait_reason_str(int wait_reason) {
    switch (wait_reason) {
        case PROC_WAIT_NONE:          return "NONE";
        case PROC_WAIT_CONSOLE_INPUT: return "CONSOLE_INPUT";
        case PROC_WAIT_CHILD_EXIT:    return "CHILD_EXIT";
        case PROC_WAIT_IPC_RECV:      return "IPC_RECV";
        default:                      return "UNKNOWN";
    }
}

static int str_len_k(const char *s) {
    int n = 0;
    while (s && s[n] != '\0') {
        n++;
    }
    return n;
}

static int append_char_k(char *out, size_t out_size, size_t *pos, char c) {
    if (!out || !pos || *pos + 1 >= out_size) {
        return -1;
    }
    out[*pos] = c;
    (*pos)++;
    out[*pos] = '\0';
    return 0;
}

static int append_str_k(char *out, size_t out_size, size_t *pos, const char *s) {
    if (!s) {
        return append_str_k(out, out_size, pos, "(null)");
    }
    while (*s) {
        if (append_char_k(out, out_size, pos, *s++) < 0) {
            return -1;
        }
    }
    return 0;
}

static int append_u32_k(char *out, size_t out_size, size_t *pos, uint32_t v) {
    char tmp[10];
    int n = 0;
    if (v == 0) {
        return append_char_k(out, out_size, pos, '0');
    }
    while (v > 0 && n < (int) sizeof(tmp)) {
        tmp[n++] = (char) ('0' + (v % 10));
        v /= 10;
    }
    for (int i = n - 1; i >= 0; i--) {
        if (append_char_k(out, out_size, pos, tmp[i]) < 0) {
            return -1;
        }
    }
    return 0;
}

static int append_key_val_u32(char *out, size_t out_size, size_t *pos,
                              const char *key, uint32_t value) {
    if (append_str_k(out, out_size, pos, key) < 0) return -1;
    if (append_str_k(out, out_size, pos, ":\t") < 0) return -1;
    if (append_u32_k(out, out_size, pos, value) < 0) return -1;
    if (append_char_k(out, out_size, pos, '\n') < 0) return -1;
    return 0;
}

static int append_key_val_str(char *out, size_t out_size, size_t *pos,
                              const char *key, const char *value) {
    if (append_str_k(out, out_size, pos, key) < 0) return -1;
    if (append_str_k(out, out_size, pos, ":\t") < 0) return -1;
    if (append_str_k(out, out_size, pos, value) < 0) return -1;
    if (append_char_k(out, out_size, pos, '\n') < 0) return -1;
    return 0;
}

struct process *process_from_trap_frame(struct trap_frame *f) {
    if (!f) {
        return NULL;
    }

    uint32_t addr = (uint32_t) f;
    for (int i = 0; i < PROCS_MAX; i++) {
        struct process *proc = &procs[i];
        uint32_t stack_base = (uint32_t) &proc->stack[0];
        uint32_t stack_end = (uint32_t) (&proc->stack[sizeof(proc->stack)]);
        if (addr >= stack_base && addr < stack_end) {
            return proc;
        }
    }

    return NULL;
}

int procfs_sync_process(const struct process *proc) {
    if (!proc) {
        return -1;
    }
    if (proc->pid <= 0 || proc->pid >= PROCS_MAX) {
        return -1;
    }

    char dir_path[FS_PATH_MAX];
    char status_path[FS_PATH_MAX];
    char content[256];
    size_t pos = 0;

    // Ensure /proc/<pid> exists (ignore EEXIST-like failures).
    dir_path[0] = '\0';
    strcat_s(dir_path, sizeof(dir_path), "/proc/");
    pos = (size_t) str_len_k(dir_path);
    if (append_u32_k(dir_path, sizeof(dir_path), &pos, (uint32_t) proc->pid) < 0) {
        return -1;
    }
    (void) fs_mkdir(dir_path);

    strcpy_s(status_path, sizeof(status_path), dir_path);
    strcat_s(status_path, sizeof(status_path), "/status");

    content[0] = '\0';
    pos = 0;
    if (append_key_val_u32(content, sizeof(content), &pos, "pid", (uint32_t) proc->pid) < 0) return -1;
    if (append_key_val_u32(content, sizeof(content), &pos, "ppid", (uint32_t) proc->parent_pid) < 0) return -1;
    if (append_key_val_str(content, sizeof(content), &pos, "name", proc->name) < 0) return -1;
    if (append_key_val_u32(content, sizeof(content), &pos, "state_id", (uint32_t) proc->state) < 0) return -1;
    if (append_key_val_str(content, sizeof(content), &pos, "state", proc_state_str(proc->state)) < 0) return -1;
    if (append_key_val_u32(content, sizeof(content), &pos, "wait_reason_id", (uint32_t) proc->wait_reason) < 0) return -1;
    if (append_key_val_str(content, sizeof(content), &pos, "wait_reason", proc_wait_reason_str(proc->wait_reason)) < 0) return -1;
    if (append_key_val_str(content, sizeof(content), &pos, "cwd", proc->cwd_path) < 0) return -1;

    int fd = fs_open(proc->pid, status_path, O_CREAT | O_WRONLY | O_TRUNC);
    if (fd < 0) {
        return -1;
    }

    int len = str_len_k(content);
    int written = fs_write(proc->pid, fd, content, (size_t) len);
    (void) fs_close(proc->pid, fd);
    return (written == len) ? 0 : -1;
}
