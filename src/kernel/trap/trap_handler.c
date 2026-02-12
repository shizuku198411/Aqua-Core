#include "kernel.h"
#include "timer.h"
#include "process.h"
#include "stdtypes.h"
#include "commonlibs.h"
#include "syscall.h"

extern struct process *current_proc;


void handle_trap(struct trap_frame *f) {
    uint32_t scause  = READ_CSR(scause);
    uint32_t stval   = READ_CSR(stval);
    uint32_t user_pc = READ_CSR(sepc);

    switch (scause) {
        // instruction address misaligned
        case SCAUSE_INSTRUCTION_ADDRESS_MISALIGNED:
            PANIC("Instruction address misaligned. scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);

        // instruction access fault
        case SCAUSE_INSTRUCTION_ACCESS_FAULT:
            PANIC("Instruction access fault. scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);

        // illecal instruction
        case SCAUSE_ILLEGAL_INSTRUCTION:
            PANIC("Illegal Instruction. scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);

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
            handle_syscall(f);
            user_pc += 4;
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
            // set next time slice
            timer_set_next();
            poll_console_input();
            // call scheduler
            if (current_proc && current_proc->state == PROC_RUNNABLE) {
                yield();
            }
            return;
        
        default:
            PANIC("unexpected trap scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);
    }

    WRITE_CSR(sepc, user_pc);
}
