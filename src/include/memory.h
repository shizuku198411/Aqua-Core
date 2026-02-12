#pragma once

#include "stdtypes.h"

#define PAGE_SIZE   4096

#define SATP_SV32   (1u << 31)      // Sv32 Mode Flag
#define PAGE_V      (1 << 0)        // enable bit
#define PAGE_R      (1 << 1)        // readable
#define PAGE_W      (1 << 2)        // writable
#define PAGE_X      (1 << 3)        // executable
#define PAGE_U      (1 << 4)        // accessable from U-Mode

void memory_init(void);
paddr_t alloc_pages(uint32_t n);
void free_pages(paddr_t paddr, uint32_t n);
void map_page(uint32_t *table1, uint32_t vaddr, paddr_t paddr, uint32_t flags);
int bitmap_page_state(int index);
int bitmap_page_count(void);
