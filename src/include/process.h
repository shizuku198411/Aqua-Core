#pragma once

#include "stdtypes.h"


#define PROCS_MAX   8
#define PROC_UNUSED     0
#define PROC_RUNNABLE   1
#define PROC_WAITTING   2
#define PROC_EXITED     3

#define PROC_WAIT_NONE          0
#define PROC_WAIT_CONSOLE_INPUT 1


struct process {
    int         pid;                // process id
    int         state;              // process status
    int         wait_reason;        // why this process is waiting
    uint32_t    user_pages;         // mapped user pages count
    vaddr_t     sp;                 // sp for context switch
    uint32_t    *page_table;        // page table
    uint8_t     stack[8192];        // kernel stack
};



extern struct process procs[PROCS_MAX];
extern struct process *current_proc;
extern struct process *idle_proc;

void switch_context(uint32_t *prev_sp, uint32_t *next_sp);
struct process *create_process(const void *image, size_t image_size);
void wakeup_input_waiters(void);
void yield(void);
