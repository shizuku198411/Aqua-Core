#include "stdtypes.h"
#include "commonlibs.h"
#include "memory.h"
#include "kernel.h"

extern char __free_ram[], __free_ram_end[];

static uint8_t *page_bitmap;
static paddr_t managed_base;
static uint32_t managed_pages;
static bool memory_initialized;

static inline bool bitmap_test(uint32_t idx) {
    return (page_bitmap[idx / 8] >> (idx % 8)) & 1;
}

static inline void bitmap_set(uint32_t idx) {
    page_bitmap[idx / 8] |= (uint8_t) (1u << (idx % 8));
}

static inline void bitmap_clear(uint32_t idx) {
    page_bitmap[idx / 8] &= (uint8_t) ~(1u << (idx % 8));
}

uint32_t memory_init(void) {
    paddr_t free_start = (paddr_t) __free_ram;
    paddr_t free_end = (paddr_t) __free_ram_end;

    printf("     [mem] check free ram range...");
    if (free_end <= free_start) {
        PANIC("invalid free ram range");
    }
    printf("OK\n");

    printf("     [mem] calculate page size...");
    uint32_t total_pages = (free_end - free_start) / PAGE_SIZE;
    if (total_pages == 0) {
        PANIC("no allocatable pages");
    }
    printf("OK\n");

    printf("     [mem] create bitmap...");
    uint32_t bitmap_bytes = (total_pages + 7) / 8;
    uint32_t bitmap_pages = (bitmap_bytes + PAGE_SIZE - 1) / PAGE_SIZE;

    if (bitmap_pages >= total_pages) {
        PANIC("bitmap too large for free ram");
    }

    page_bitmap = (uint8_t *) free_start;
    memset(page_bitmap, 0, bitmap_pages * PAGE_SIZE);
    printf("OK\n");

    managed_base = free_start + bitmap_pages * PAGE_SIZE;
    managed_pages = total_pages - bitmap_pages;
    memory_initialized = true;
    
    return total_pages;
}

paddr_t alloc_pages(uint32_t n) {
    if (!memory_initialized) {
        PANIC("memory allocator is not initialized");
    }
    if (n == 0 || n > managed_pages) {
        PANIC("invalid alloc page count %d", n);
    }

    uint32_t run = 0;
    for (uint32_t i = 0; i < managed_pages; i++) {
        if (bitmap_test(i)) {
            run = 0;
            continue;
        }

        run++;
        if (run == n) {
            uint32_t start = i + 1 - n;
            for (uint32_t j = start; j <= i; j++) {
                bitmap_set(j);
            }

            paddr_t paddr = managed_base + start * PAGE_SIZE;
            memset((void *) paddr, 0, n * PAGE_SIZE);
            return paddr;
        }
    }

    PANIC("Out of Memory has been detected.");
}

void free_pages(paddr_t paddr, uint32_t n) {
    if (!memory_initialized) {
        PANIC("memory allocator is not initialized");
    }
    if (n == 0) {
        PANIC("invalid free page count 0");
    }
    if (!is_aligned(paddr, PAGE_SIZE)) {
        PANIC("unaligned free paddr %x", paddr);
    }
    if (paddr < managed_base) {
        PANIC("free before managed base %x", paddr);
    }

    uint32_t start = (paddr - managed_base) / PAGE_SIZE;
    if (start >= managed_pages || start + n > managed_pages) {
        PANIC("free out of range paddr=%x n=%d", paddr, n);
    }

    for (uint32_t i = 0; i < n; i++) {
        uint32_t idx = start + i;
        if (!bitmap_test(idx)) {
            PANIC("double free detected paddr=%x", paddr + i * PAGE_SIZE);
        }
        bitmap_clear(idx);
    }
}

void map_page(uint32_t *table1, uint32_t vaddr, paddr_t paddr, uint32_t flags) {
    if (!is_aligned(vaddr, PAGE_SIZE)) {
        PANIC("unaligned vaddr %x", vaddr);
    }
    if (!is_aligned(paddr, PAGE_SIZE)) {
        PANIC("unaligned paddr %x", paddr);
    }

    uint32_t vpn1 = (vaddr >> 22) & 0x3ff;
    if ((table1[vpn1] & PAGE_V) == 0) {
        uint32_t pt_paddr = alloc_pages(1);
        table1[vpn1] = ((pt_paddr / PAGE_SIZE) << 10) | PAGE_V;
    }

    uint32_t vpn0 = (vaddr >> 12) & 0x3ff;
    uint32_t *table0 = (uint32_t *) ((table1[vpn1] >> 10) * PAGE_SIZE);
    table0[vpn0] = ((paddr / PAGE_SIZE) << 10) | flags | PAGE_V;
}

int bitmap_page_state(int index) {
    if (!memory_initialized) {
        PANIC("memory allocator is not initialized");
    }
    if (index < 0 || (uint32_t) index >= managed_pages) {
        return -1;
    }
    return bitmap_test((uint32_t) index) ? 1 : 0;
}

int bitmap_page_count(void) {
    if (!memory_initialized) {
        PANIC("memory allocator is not initialized");
    }
    return (int) managed_pages;
}
