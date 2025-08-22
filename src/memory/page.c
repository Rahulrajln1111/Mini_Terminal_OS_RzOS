#include "memory.h"
#include <stdint.h>

/* Simple bitmap single-page allocator for identity-mapped i386 kernel.
   Places bitmap right after the kernel_end passed to memory_init(). */

static uint8_t *page_bitmap = 0;    /* 1 bit per page */
static size_t total_pages = 0;
static size_t free_pages_count = 0;
static uintptr_t phys_mem_start = 0; /* base physical address we manage */

/* bit ops */
static inline void set_bit(size_t idx)  { page_bitmap[idx >> 3] |= (1u << (idx & 7)); }
static inline void clear_bit(size_t idx){ page_bitmap[idx >> 3] &= ~(1u << (idx & 7)); }
static inline int  test_bit(size_t idx) { return (page_bitmap[idx >> 3] >> (idx & 7)) & 1u; }

void memory_init(uintptr_t kernel_end,uint32_t mem_size)
{
    /* assume physical RAM starts at 1 MiB for typical hobby kernels */
    phys_mem_start = kernel_end;
    total_pages = mem_size / PAGE_SIZE;

    /* place bitmap right after kernel_end, aligned to page */
    uintptr_t bm_start = (kernel_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    size_t bitmap_bytes = (total_pages + 7) / 8;

    page_bitmap = (uint8_t *)bm_start;

    /* zero bitmap */
    memset(page_bitmap, 0, bitmap_bytes);

    /* mark pages used by kernel + bitmap itself as used */
    uintptr_t used_region_end = bm_start + bitmap_bytes;
    size_t kernel_pages = (used_region_end - phys_mem_start + PAGE_SIZE - 1) / PAGE_SIZE;
    for (size_t i = 0; i < kernel_pages && i < total_pages; ++i) {
        set_bit(i);
    }

    /* count free pages */
    free_pages_count = total_pages;
    for (size_t i = 0; i < total_pages; ++i) {
        if (test_bit(i)) free_pages_count--;
    }
}

void *alloc_page(void)
{
    for (size_t i = 0; i < total_pages; ++i) {
        if (!test_bit(i)) {
            set_bit(i);
            free_pages_count--;
            uintptr_t pa = phys_mem_start + i * PAGE_SIZE;
            return (void *)pa; /* identity-mapped */
        }
    }
    return 0;
}

void free_page(void *p)
{
    if (!p) return;
    uintptr_t pa = (uintptr_t)p;
    if (pa < phys_mem_start) return;
    size_t idx = (pa - phys_mem_start) / PAGE_SIZE;
    if (idx < total_pages && test_bit(idx)) {
        clear_bit(idx);
        free_pages_count++;
    }
}

uintptr_t page_to_phys(void *p) { return (uintptr_t)p; }
void *phys_to_virt(uintptr_t p) { return (void*)p; }

size_t mem_total_pages(void) { return total_pages; }
size_t mem_free_pages(void) { return free_pages_count; }
