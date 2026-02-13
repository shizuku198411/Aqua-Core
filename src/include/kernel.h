#pragma once

#include "stdtypes.h"


// macro: kernel panic
#define PANIC(fmt, ...)                                                         \
    do {                                                                        \
        printf("PANIC: %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);   \
        for (;;) {                                                              \
            __asm__ __volatile__("wfi");                                        \
        }                                                                       \
    } while (0)


// macro: read csr registry
#define READ_CSR(reg)                                                          \
    ({                                                                         \
        unsigned long __tmp;                                                   \
        __asm__ __volatile__("csrr %0, " #reg : "=r"(__tmp));                  \
        __tmp;                                                                 \
    })


// macro: write csr registry
#define WRITE_CSR(reg, value)                                                  \
    do {                                                                       \
        uint32_t __tmp = (value);                                              \
        __asm__ __volatile__("csrw " #reg ", %0" ::"r"(__tmp));                \
    } while (0)


// scause
// doc: 4.1.9 Supervisor Trap Value (stval) Register
#define SCAUSE_INSTRUCTION_ADDRESS_MISALIGNED   0x00
#define SCAUSE_INSTRUCTION_ACCESS_FAULT         0x01
#define SCAUSE_ILLEGAL_INSTRUCTION              0x02
#define SCAUSE_BREAKPOINT                       0x03
#define SCAUSE_LOAD_ADDRESS_MISALIGNED          0x04
#define SCAUSE_LOAD_ACCESS_FAULT                0x05
#define SCAUSE_STORE_AMO_ADDRESS_MISALIGNED     0x06
#define SCAUSE_STORE_AMO_ACCESS_FAULT           0x07
#define SCAUSE_ENVIRONMENT_CALL_FROM_U_MODE     0x08
#define SCAUSE_ENVIRONMENT_CALL_FROM_S_MODE     0x09
// 0x0a: Reserved
// 0x0b: Reserved
#define SCAUSE_INSTRUCTION_PAGE_FAULT           0x0c
#define SCAUSE_LOAD_PAGE_FAULT                  0x0d
// 0x0e: Reserved
#define SCAUSE_STORE_AMO_PAGE_FAULT             0x0f

#define SCAUSE_SUPERVISOR_TIMER                 0x80000005


// trap frame structure
struct trap_frame {
    uint32_t ra;
    uint32_t gp;
    uint32_t tp;
    uint32_t t0;
    uint32_t t1;
    uint32_t t2;
    uint32_t t3;
    uint32_t t4;
    uint32_t t5;
    uint32_t t6;
    uint32_t a0;
    uint32_t a1;
    uint32_t a2;
    uint32_t a3;
    uint32_t a4;
    uint32_t a5;
    uint32_t a6;
    uint32_t a7;
    uint32_t s0;
    uint32_t s1;
    uint32_t s2;
    uint32_t s3;
    uint32_t s4;
    uint32_t s5;
    uint32_t s6;
    uint32_t s7;
    uint32_t s8;
    uint32_t s9;
    uint32_t s10;
    uint32_t s11;
    uint32_t sp;
} __attribute__((packed));

#define KERNEL_BASE 0x80200000
#define USER_BASE 0x1000000
#define SSTATUS_SPIE    (1 << 5)
#define KERNEL_VERSION_MAX 16

struct kernel_info {
    char version[KERNEL_VERSION_MAX];
    uint32_t total_pages;
    uint32_t page_size;
    uint32_t kernel_base;
    uint32_t user_base;
    uint32_t proc_max;
    uint32_t kernel_stack_bytes;
    uint32_t time_slice_ticks;
    uint32_t timer_interval_ms;
};

void user_entry(void);
__attribute__((noreturn)) void kernel_shutdown(void);
void kernel_get_info(struct kernel_info *out);
