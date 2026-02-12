#pragma once

#include "stdtypes.h"


#define PROCS_MAX   64

// status
#define PROC_UNUSED     0
#define PROC_RUNNABLE   1
#define PROC_WAITTING   2
#define PROC_EXITED     3

// wait reason
#define PROC_WAIT_NONE          0
#define PROC_WAIT_CONSOLE_INPUT 1
#define PROC_WAIT_CHILD_EXIT    2
#define PROC_WAIT_IPC_RECV      3

#define SCHED_TIME_SLICE_TICKS  3


struct process {
    int         pid;                // process id
    int         state;              // process status
    int         wait_reason;        // why this process is waiting
    int         wait_pid;           // target child pid for waitpid (-1:any)
    int         parent_pid;         // parent pid (0: no parent)
    uint32_t    user_pages;         // mapped user pages count
    vaddr_t     sp;                 // sp for context switch
    uint32_t    *page_table;        // page table
    uint32_t    time_slice;         // remaining time slice ticks
    uint32_t    run_ticks;          // accumulated running ticks
    uint32_t    schedule_count;     // how many times scheduled in
    int         ipc_has_message;    // single-slot mailbox state
    int         ipc_from_pid;       // mailbox sender pid
    uint32_t    ipc_message;        // mailbox payload
    uint8_t     stack[8192];        // kernel stack
};


extern struct process procs[PROCS_MAX];
extern struct process *current_proc;
extern struct process *idle_proc;
extern struct process *init_proc;

void switch_context(uint32_t *prev_sp, uint32_t *next_sp);
struct process *create_process(const void *image, size_t image_size);
void wakeup_input_waiters(void);
void notify_child_exit(struct process *child);
void orphan_children(int parent_pid);
int wait_for_child_exit(int parent_pid, int target_pid);
void scheduler_on_timer_tick(void);
bool scheduler_should_yield(void);
int process_ipc_send(int src_pid, int dst_pid, uint32_t message);
int process_ipc_recv(int self_pid, int *from_pid, uint32_t *message);
void yield(void);
