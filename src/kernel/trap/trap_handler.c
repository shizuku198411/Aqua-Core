#include "kernel/kernel.h"
#include "time/timer.h"
#include "proc/process.h"
#include "core/stdtypes.h"
#include "core/commonlibs.h"
#include "user/syscall.h"
#include "kernel/page_access.h"

extern struct process *current_proc;


void handle_trap(struct trap_frame *f) {
    uint32_t scause  = READ_CSR(scause);
    uint32_t stval   = READ_CSR(stval);
    uint32_t user_pc = READ_CSR(sepc);
    uint32_t sstatus = READ_CSR(sstatus);
    bool from_user = (sstatus & (1u << 8)) == 0;
    struct process *owner = NULL;

    if (from_user) {
        owner = process_from_trap_frame(f);
        if (owner) {
            current_proc = owner;
        }
    }

    switch (scause) {
        // instruction address misaligned
        case SCAUSE_INSTRUCTION_ADDRESS_MISALIGNED:
            PANIC("Instruction address misaligned. scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);

        // instruction access fault
        case SCAUSE_INSTRUCTION_ACCESS_FAULT:
            PANIC("Instruction access fault. scause=%x, stval=%x, sepc=%x, from_user=%d, pid=%d, name=%s, ra=%x\n",
                  scause,
                  stval,
                  user_pc,
                  (int) from_user,
                  current_proc ? current_proc->pid : -1,
                  current_proc ? current_proc->name : "none",
                  f ? f->ra : 0);

        // illecal instruction
        case SCAUSE_ILLEGAL_INSTRUCTION:
            {
                uint16_t insn16 = 0;
                uint16_t next16 = 0;
                uint16_t prev16 = 0;
                if (from_user) {
                    (void) copyin(&insn16, (const void *) user_pc, sizeof(insn16));
                    (void) copyin(&next16, (const void *) (user_pc + 2u), sizeof(next16));
                    if (user_pc >= (USER_BASE + 2u)) {
                        (void) copyin(&prev16, (const void *) (user_pc - 2u), sizeof(prev16));
                    }
                }

            PANIC("Illegal Instruction. scause=%x, stval=%x, sepc=%x, from_user=%d, pid=%d, name=%s, prev=%x, insn0=%x, insn1=%x\n",
                  scause,
                  stval,
                  user_pc,
                  (int) from_user,
                  current_proc ? current_proc->pid : -1,
                  current_proc ? current_proc->name : "none",
                  (unsigned) prev16,
                  (unsigned) insn16,
                  (unsigned) next16);
            }

        // breakpoint
        case SCAUSE_BREAKPOINT:
            PANIC("Breakpoint. scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);

        // load address misaligned
        case SCAUSE_LOAD_ADDRESS_MISALIGNED:
            PANIC("Load address misaligned. scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);

        // load access fault
        case SCAUSE_LOAD_ACCESS_FAULT:
            PANIC("Load access fault. scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);

        // store/AMO address misaligned
        case SCAUSE_STORE_AMO_ADDRESS_MISALIGNED:
            PANIC("Store/AMO address misaligned. scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);

        // store/AMO access fault
        case SCAUSE_STORE_AMO_ACCESS_FAULT:
            PANIC("Store/AMO access fault. scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);

        // environment call from U-Mode
        case SCAUSE_ENVIRONMENT_CALL_FROM_U_MODE:
            {
                uint32_t sysno = f->a3;
                handle_syscall(f);
                // exec-family succeeds with f->a0 == 0.
                // In that case, restart from the new image entry point.
                if ((sysno == SYSCALL_EXEC_PATH ||
                     sysno == SYSCALL_EXECV_PATH) &&
                    f->a0 == 0) {
                    user_pc = USER_BASE;
                } else {
                    user_pc += 4;
                }
            }
            break;

        // environment call from S-Mode
        case SCAUSE_ENVIRONMENT_CALL_FROM_S_MODE:
            PANIC("Environment call from S-Mode. scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);

        // instruction page fault
        case SCAUSE_INSTRUCTION_PAGE_FAULT:
            PANIC("Instruction page fault. scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);

        // load page fault
        case SCAUSE_LOAD_PAGE_FAULT:
            PANIC("Load page fault. scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);

        // store/ANO page fault
        case SCAUSE_STORE_AMO_PAGE_FAULT:
            PANIC("Store/AMO page fault. scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);

        // timer interrupt
        case SCAUSE_SUPERVISOR_TIMER:
            timer_set_next();
            poll_console_input();
            scheduler_on_timer_tick();
            if (scheduler_should_yield()) {
                yield();
            }
            // Timer trap can context-switch away and resume this trap frame later.
            // Restore this frame's resume PC explicitly to avoid returning with
            // a sepc value clobbered by another trap in between.
            WRITE_CSR(sepc, user_pc);
            if (current_proc && current_proc->pid > 0) {
                WRITE_CSR(sscratch, (uint32_t) &current_proc->stack[sizeof(current_proc->stack)]);
            } else if (owner) {
                WRITE_CSR(sscratch, (uint32_t) &owner->stack[sizeof(owner->stack)]);
            }
            return;

        default:
            PANIC("unexpected trap scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);
    }

    if (current_proc && current_proc->pid > 0) {
        WRITE_CSR(sscratch, (uint32_t) &current_proc->stack[sizeof(current_proc->stack)]);
    } else if (owner) {
        WRITE_CSR(sscratch, (uint32_t) &owner->stack[sizeof(owner->stack)]);
    }
    WRITE_CSR(sepc, user_pc);
}
