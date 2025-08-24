#ifndef PAGE_H
#define PAGE_H

#include <stdint.h>
#include <stddef.h>

/* Page table/directory entry flags */
#define PAGE_PRESENT   0x1
#define PAGE_RW        0x2
#define PAGE_USER      0x4

/* Backwards compatibility/aliases for the existing code */
#define PDE_PRESENT    0x1
#define PDE_RW         0x2
#define PTE_PRESENT    0x1
#define PTE_RW         0x2

/* Functions */
void page_init(void);
int  map_page(uintptr_t va, uintptr_t pa);      // simple map RW, kernel-only
int  unmap_page(uintptr_t va);
uintptr_t virt_to_phys(uintptr_t va);


#endif
