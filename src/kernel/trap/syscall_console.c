#include "syscall_internal.h"
#include "syscall.h"
#include "process.h"
#include "commonlibs.h"

extern struct process *current_proc;

static char input_buf[64];
static uint32_t input_head;
static uint32_t input_tail;
static uint32_t input_count;

static bool input_pop(char *ch) {
    if (input_count == 0) {
        return false;
    }

    *ch = input_buf[input_head];
    input_head = (input_head + 1) % sizeof(input_buf);
    input_count--;
    return true;
}

void poll_console_input(void) {
    while (input_count < sizeof(input_buf)) {
        long ch = getchar();
        if (ch < 0) {
            break;
        }

        input_buf[input_tail] = (char) ch;
        input_tail = (input_tail + 1) % sizeof(input_buf);
        input_count++;
    }

    if (input_count > 0) {
        wakeup_input_waiters();
    }
}

void syscall_handle_putchar(struct trap_frame *f) {
    putchar(f->a0);
}

void syscall_handle_getchar(struct trap_frame *f) {
    char ch;
    while (!input_pop(&ch)) {
        poll_console_input();
        if (input_pop(&ch)) {
            break;
        }

        current_proc->wait_reason = PROC_WAIT_CONSOLE_INPUT;
        current_proc->state = PROC_WAITTING;
        yield();
    }

    current_proc->wait_reason = PROC_WAIT_NONE;
    f->a0 = (uint8_t) ch;
}
