#include <stdint.h>
#include <stddef.h>
#include "memory/page.h"
#include "utils.h"
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
uintptr_t pd_phys_global;
uintptr_t kernel_pd_phys = 0;
/* Exported allocator prototype: we'll cast returned pointer to uint32_t. */
extern void* alloc_page(void);

/* Use the existing serial print function from shell */
extern void print_serial(char *s);


/* Globals visible for GDB and PF handler */
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
    // Get the physical address of the kernel's page directory from CR3
    uint32_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));

    // Get the indices
    uint32_t pd_index = va >> 22;
    uint32_t pt_index = (va >> 12) & 0x03FF;
    uint32_t offset = va & 0xFFF;

    // Get the page directory entry
    uint32_t* pd = (uint32_t*) (cr3 + pd_index * 4);
    if (!(*pd & PAGE_PRESENT)) {
        return 0; // Page Directory Entry not present
    }

    // Get the page table entry
    uint32_t* pt = (uint32_t*)((*pd & ~0xFFF) + pt_index * 4);
    if (!(*pt & PAGE_PRESENT)) {
        return 0; // Page Table Entry not present
    }

    // Return the physical address
    return (*pt & ~0xFFF) + offset;
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

    // Identity map first 4 MB
map_page(0x00000000, 0x00000000);

// Map kernel: virtual 0xC0100000 -> physical 0x00100000
map_page(0xC0100000, 0x00100000);



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


// Function to support process creation

/**
 * Creates and returns the physical address of a new page directory.
 * The new page directory is a clone of the kernel's mappings.
 */
uintptr_t create_page_directory(void) {
    // 1. Allocate a physical page for the new page directory
    uintptr_t new_pd_phys = (uintptr_t)alloc_page();
    if (new_pd_phys == 0) {
        return 0; // Allocation failed
    }

    // 2. Map the new page directory into a temporary virtual address
    //    so we can work with it. We'll use a known virtual address for this.
    uintptr_t new_pd_virt = KERNEL_TEMP_MAP; // Define this in your kernel headers
    map_page(new_pd_virt, new_pd_phys);

    // 3. Clear the entire page directory
    uint32_t *new_pd = (uint32_t*)new_pd_virt;
    memset(new_pd, 0, PAGE_SIZE);

    // 4. Copy the kernel's page directory entries (PDEs)
    //    from the master kernel page directory.
    //    Kernel mappings start at 0xC0000000, which is PDE index 768.
    uint32_t *kernel_pd = (uint32_t*)KERNEL_PD_VIRT;

    // There are 256 kernel PDEs from 768 to 1023 (1024 - 768 = 256)
    for (int i = 768; i < 1024; ++i) {
        new_pd[i] = kernel_pd[i];
    }
    
    // Unmap the temporary virtual address
    unmap_page(new_pd_virt);
    
    return new_pd_phys;
}

// A function to temporarily map a physical page to a virtual address
void* map_page_to_virt(uintptr_t pa) {
    // We get the physical address of the page directory from CR3
    uint32_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));

    // Get the PDE and PTE for the temporary mapping address
    uint32_t pd_index = (KERNEL_TEMP_MAP >> 22);
    uint32_t pt_index = (KERNEL_TEMP_MAP >> 12) & 0x03FF;

    // The PDE for the temporary mapping (pointing to a page table)
    uint32_t* pd = (uint32_t*)0xFFFFF000;
    uint32_t* pt = (uint32_t*)(0xFFC00000 | (pd_index << 12));

    // Ensure the page table exists
    if (!(pd[pd_index] & PAGE_PRESENT)) {
        // Handle this error or create a new page table
        return 0;
    }

    // Map the physical page at the temporary virtual address
    pt[pt_index] = (pa & ~0xFFF) | PAGE_PRESENT | PAGE_RW;

    return (void*)KERNEL_TEMP_MAP;
}


int map_page_to(uintptr_t pd_phys, uintptr_t va, uintptr_t pa, uint32_t flags) {
    // Map the target page directory into our temporary virtual address
    uint32_t* target_pd = (uint32_t*)map_page_to_virt(pd_phys);
    if (!target_pd) {
        return -1; // Failed to map page directory
    }

    // Calculate the indices
    uint32_t pd_index = va >> 22;
    uint32_t pt_index = (va >> 12) & 0x3ff;
    
    // Check if the page table exists
    uintptr_t pt_phys = target_pd[pd_index] & 0xFFFFF000;
    if (!pt_phys) {
        // It doesn't, so allocate and map a new page table
        uintptr_t new_pt_phys = (uintptr_t)alloc_page();
        if (!new_pt_phys) return -1;
        
        // Map the new page table at a temporary virtual address
        uint32_t* new_pt_virt = (uint32_t*)map_page_to_virt(new_pt_phys);
        if (!new_pt_virt) return -1;

        // Clear the new page table
        memset(new_pt_virt, 0, PAGE_SIZE);

        // Update the page directory entry
        target_pd[pd_index] = (uint32_t)new_pt_phys | PAGE_PRESENT | PAGE_RW | PAGE_USER;
        
        // Unmap the temporary address for the new page table
        unmap_page((uintptr_t)new_pt_virt);
        
        pt_phys = new_pt_phys;
    }

    // Now, map the physical page in the page table
    uint32_t* pt = (uint32_t*)map_page_to_virt(pt_phys);
    if (!pt) return -1;
    
    pt[pt_index] = (uint32_t)pa | flags;
    
    // Unmap the temporary address for the page table
    unmap_page((uintptr_t)pt);
    
    // Unmap the temporary address for the page directory
    unmap_page((uintptr_t)target_pd);

    return 0;
}