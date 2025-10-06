#ifndef PAGE_H
#define PAGE_H

#include <stdint.h>
#include <stddef.h>


void memory_init(uint32_t mem_size, uintptr_t kernel_end);
void *alloc_page(void);
void free_page(void *p);
uintptr_t page_to_phys(void *p);   // if you use phys==virt, returns same
void *phys_to_virt(uintptr_t p);   // identity for now

// debugging
size_t mem_total_pages(void);
size_t mem_free_pages(void);

#endif
