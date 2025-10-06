/* src/memory/page.c
   Minimal, robust paging init for 32-bit kernel:
   - identity map 0..4MB (covers kernel at 1MB + typical stack)
   - recursive mapping at PDE[1023]
   - load CR3 with PD physical address
   - enable CR0.PG and do a short jmp to flush
   Uses only print_serial() for debug output.
*/

#include <stdint.h>
#include <stddef.h>
#include "memory/page.h"
#include "memory/memory.h" /* alloc_page() should return 4KB-aligned PHYS pointer */

/* If page flags are not defined in your page.h, define fallbacks here */
#ifndef PDE_PRESENT
#define PDE_PRESENT  0x1
#endif
#ifndef PDE_RW
#define PDE_RW       0x2
#endif
#ifndef PTE_PRESENT
#define PTE_PRESENT  0x1
#endif
#ifndef PTE_RW
#define PTE_RW       0x2
#endif

/* Exported allocator prototype: we'll cast returned pointer to uint32_t. */
extern void* alloc_page(void);

/* Use the existing serial print function from shell */
extern void print_serial(char *s);

/* tiny hex printer that writes "0xXXXXXXXX" via print_serial */
static void print_hex(uint32_t x) {
    char b[11];
    static const char hex[] = "0123456789ABCDEF";
    b[0] = '0'; b[1] = 'x';
    for (int i = 0; i < 8; ++i) {
        uint32_t shift = (7 - i) * 4;
        b[2 + i] = hex[(x >> shift) & 0xF];
    }
    b[10] = '\0';
    print_serial(b);
}

/* small aliases to match existing code's kputs/kputhex usage */
#define kputs(s) print_serial((char*)(s))
#define kputhex(x) print_hex((uint32_t)(x))

/* Globals visible for GDB and PF handler */
uint32_t pd_phys_global = 0;
volatile uint32_t last_cr2 = 0;

/* Helpers: read/write CR registers */
static inline void write_cr3(uint32_t v){ __asm__ volatile("mov %0, %%cr3" :: "r"(v) : "memory"); }
static inline uint32_t read_cr0(void){ uint32_t r; __asm__ volatile("mov %%cr0,%0":"=r"(r)); return r; }
static inline void write_cr0(uint32_t v){ __asm__ volatile("mov %0, %%cr0" :: "r"(v) : "memory"); }
static inline void invlpg(void* addr){ __asm__ volatile("invlpg (%0)" :: "r"(addr) : "memory"); }


/* Zero 4KiB page at physical address 'phys' */
static void zero_page_phys(uint32_t phys) {
    /* We assume identity-mapped access to physical memory before paging is enabled. */
    uint32_t *p = (uint32_t*)(uintptr_t)phys;
    for (int i = 0; i < 1024; ++i) p[i] = 0;
}

/* ---------------------------
   RENAMED internal helper:
   Map a single 4K page: va -> pa with flags (PTE flags, low bits)
   pd_phys: physical address of page directory (physical)
   --------------------------- */
static void map_page_phys(uint32_t pd_phys, uint32_t va, uint32_t pa, uint32_t flags)
{
    /* Directly manipulate physical PD/PT before paging is active (identity access). */
    uint32_t *pd = (uint32_t*)(uintptr_t)pd_phys;
    uint32_t dir = (va >> 22) & 0x3FF;
    uint32_t tbl = (va >> 12) & 0x3FF;

    uint32_t pde = pd[dir];
    uint32_t pt_phys;
    if (!(pde & PDE_PRESENT)) {
        /* allocate a new page table (physical) */
        pt_phys = (uint32_t)alloc_page(); /* alloc_page must return a physical page addr (void*) */
        zero_page_phys(pt_phys);
        pd[dir] = (pt_phys & ~0xFFFu) | PDE_PRESENT | PDE_RW;
    } else {
        pt_phys = pde & ~0xFFFu;
    }

    uint32_t *pt = (uint32_t*)(uintptr_t)pt_phys;
    pt[tbl] = (pa & ~0xFFFu) | (flags & 0xFFFu) | PTE_PRESENT;
    /* No invlpg here — CR3 write will flush TLB when paging enabled. */
}

/* Map a contiguous range (size rounded up to page).
   pd_phys: page-directory physical base
   va: virtual address base
   pa: physical address base
   size: size in bytes
*/

// in page.c


int map_page(uintptr_t va, uintptr_t pa) {
    if (!pd_phys_global) {
        return -1; // Paging not initialized
    }
    map_page_phys(pd_phys_global, va, pa, PTE_PRESENT | PTE_RW);
    return 0; // Success
}

/* Implements the unmap_page() function. */
int unmap_page(uintptr_t va) {
    uint32_t pd_index = (va >> 22) & 0x3FF;
    uint32_t pt_index = (va >> 12) & 0x3FF;

    uint32_t *pd = (uint32_t*)0xFFC00000;
    if (!(pd[pd_index] & PDE_PRESENT)) {
        return -1; // Page directory entry not present
    }

    uint32_t *pt = (uint32_t*)(0xFFC00000 | (pd_index << 12));
    pt[pt_index] = 0; // Clear the page table entry

    invlpg((void*)va); // Flush the TLB for this virtual address

    return 0; // Success
}

/* Implements the virt_to_phys() function. */
uintptr_t virt_to_phys(uintptr_t va) {
    uint32_t pd_index = (va >> 22) & 0x3FF;
    uint32_t pt_index = (va >> 12) & 0x3FF;

    uint32_t *pd = (uint32_t*)0xFFC00000;
    if (!(pd[pd_index] & PDE_PRESENT)) {
        return 0; // PDE not present
    }

    uint32_t *pt = (uint32_t*)(0xFFC00000 | (pd_index << 12));
    if (!(pt[pt_index] & PTE_PRESENT)) {
        return 0; // PTE not present
    }

    uint32_t pa = (pt[pt_index] & ~0xFFF) | (va & 0xFFF);
    return pa;
}




static void map_range(uint32_t pd_phys, uint32_t va, uint32_t pa, uint32_t size, uint32_t flags) {
    uint32_t end = pa + size;
    for (uint32_t cur_pa = pa, cur_va = va; cur_pa < end; cur_pa += PAGE_SIZE, cur_va += PAGE_SIZE) {
        map_page_phys(pd_phys, cur_va, cur_pa, flags);
    }
}

/* Public: initialize paging and enable it safely (identity-map low 4MB,
   set recursive mapping, map VGA, load CR3, set PG) */
void page_init(void)
{
    kputs("paging: start\n");

    /* allocate page directory (physical) — alloc_page must return a PHYSICAL page aligned to 4K */
    // uint32_t pd_phys = (uint32_t)alloc_page();
    // pd_phys_global = pd_phys;
    // zero_page_phys(pd_phys);

    /* allocate PT0 and identity map 0..4MB (covers kernel at 0x00100000 & stack below 4MB) */
    // uint32_t pt0_phys = (uint32_t)alloc_page();
    // zero_page_phys(pt0_phys);

    // for (uint32_t i = 0; i < 1024; ++i) {
    //     uint32_t pa = i << 12;
    //     ((uint32_t*)(uintptr_t)pt0_phys)[i] = (pa & ~0xFFFu) | PTE_PRESENT | PTE_RW;
    // }

    // /* PDE[0] -> pt0_phys */
    // ((uint32_t*)(uintptr_t)pd_phys)[0] = (pt0_phys & ~0xFFFu) | PDE_PRESENT | PDE_RW;

    // /* Optional: map VGA text (0xB8000) identity so serial/VGA works after PG=1 */
    // map_page_phys(pd_phys, 0x000B8000, 0x000B8000, PTE_RW);

    // /* set recursive mapping: PDE[1023] = pd_phys */
    // ((uint32_t*)(uintptr_t)pd_phys)[1023] = (pd_phys & ~0xFFFu) | PDE_PRESENT | PDE_RW;
    // pd_phys_global = pd_phys; /* keep global up to date for debugger */


//adding new page-directory management

 /* Allocate page directory (physical) */
    pd_phys_global = (uint32_t)alloc_page();
    zero_page_phys((uint32_t)pd_phys_global);

    /* Allocate a page table and identity map 0-4MB */
    uint32_t pt0_phys = (uint32_t)alloc_page();
    zero_page_phys(pt0_phys);

    for (uint32_t i = 0; i < 1024; ++i) {
        uint32_t pa = i << 12;
        ((uint32_t*)(uintptr_t)pt0_phys)[i] = (pa & ~0xFFFu) | PTE_PRESENT | PTE_RW;
    }

    /* Set PDE[0] to point to the page table for 0-4MB */
    ((uint32_t*)(uintptr_t)pd_phys_global)[0] = (pt0_phys & ~0xFFFu) | PDE_PRESENT | PDE_RW;

    /* Set recursive mapping: PDE[1023] = pd_phys_global */
    ((uint32_t*)(uintptr_t)pd_phys_global)[1023] = ((uint32_t)pd_phys_global & ~0xFFFu) | PDE_PRESENT | PDE_RW;

    /* Map the heap's virtual address range */
    for (uintptr_t va = KHEAP_START; va < KHEAP_END; va += PAGE_SIZE) {
        uint32_t *pd = (uint32_t*)0xFFC00000;
        uint32_t pd_index = (va >> 22) & 0x3FF;

        if (!(pd[pd_index] & PDE_PRESENT)) {
            uint32_t *pt_phys = (uint32_t*)alloc_page();
            pd[pd_index] = (uint32_t)pt_phys | PDE_PRESENT | PDE_RW;
        }
    }




    /* Debug: print PD physical */
    kputs("paging: pd_phys = ");
    kputhex(pd_phys_global);
    kputs("\n");

    /* Load CR3 with PHYSICAL pd address */
    write_cr3(pd_phys_global);
    kputs("paging: loaded cr3\n");

    /* Enable paging: set CR0.PG bit */
    uint32_t cr0 = read_cr0();
    cr0 |= 0x80000000u;
    write_cr0(cr0);

    /* Short jump to flush prefetch / instruction pipeline (recommended) */
    __asm__ volatile("jmp 1f\n1:\n");

    kputs("paging: enabled\n");
}

/* Called by your PF ISR to save CR2 (useful while debugging) */
void save_cr2_for_debug(void) {
    __asm__ volatile("mov %%cr2, %0" : "=r"(last_cr2) );
}
