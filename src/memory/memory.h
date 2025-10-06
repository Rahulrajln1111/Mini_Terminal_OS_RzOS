#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

/* kheap constants */
#define KHEAP_START 0xC0200000
#define KHEAP_END   0xC0300000   
#define KHEAP_SIZE  (16 * 1024 * 1024u) /* e.g. 16 MB initial heap */

extern uintptr_t next_heap_va;
extern uintptr_t kheap_end_va;
extern uintptr_t pd_phys_global;

/* Basic memory utils (implemented in memutils.c) */
void *memset(void *dest, int value, size_t count);
void *memcpy(void *dest, const void *src, size_t count);

/* Page allocator API (page.c) */
#define PAGE_SIZE 4096U
void *alloc_page_mapped(void);
void memory_init(uint32_t mem_size, uintptr_t kernel_end);
void kheap_init(uintptr_t kernel_end);
void *kmalloc(size_t size);
void kfree(void *ptr);
void memory_init(uint32_t mem_size, uintptr_t kernel_end);
void *alloc_page(void);
void free_page(void *p);
uintptr_t page_to_phys(void *p);
void *phys_to_virt(uintptr_t p);
size_t mem_total_pages(void);
size_t mem_free_pages(void);



#endif /* MEMORY_H */
