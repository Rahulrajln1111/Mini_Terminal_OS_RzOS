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
#define KERNEL_PD_VIRT 0xFFC00000
#define KERNEL_TEMP_MAP 0xFFBFFFFC

/* Functions */
void page_init(void);
int  map_page(uintptr_t va, uintptr_t pa);      // simple map RW, kernel-only
int  unmap_page(uintptr_t va);
uintptr_t virt_to_phys(uintptr_t va);
int map_page_to(uintptr_t pd_phys, uintptr_t va, uintptr_t pa, uint32_t flags);
void* map_page_to_virt(uintptr_t pa) ;
uintptr_t create_page_directory(void) ;


#endif
