#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "memory/page.h"
#include "utils.h" // For print_serial, itoa if needed
#include "memory/memory.h" // For kheap_alloc and kheap_free (used for page allocation)
#include "status.h" // For error codes like -EINVARG

// --- External Prototypes and Stubs (replace with your actual implementations if they exist) ---
extern void print_serial(char *s);
extern void* kmalloc(size_t size);
extern void kfree(void* ptr);
extern void* memset(void* ptr, int c, size_t size);

// A simple page frame allocator stub (returns a 4KB aligned physical address)
void* alloc_page(void) {
    void* page = kmalloc(PAGE_SIZE); 
    if (page) {
        memset(page, 0, PAGE_SIZE);
    }
    return page;
}

// A simple page frame deallocator stub
void free_page(void* ptr) {
    kfree(ptr);
}
// -----------------------------------------------------------------------------------------


// Global variable to hold the PHYSICAL address of the currently active page directory.
uint32_t *current_page_directory_phys = NULL;


/* Zero 4KiB page at physical address 'phys' */
static void zero_page_phys(uint32_t phys) {
    uint32_t *p = (uint32_t*)(uintptr_t)phys;
    for (int i = 0; i < PAGE_SIZE / sizeof(uint32_t); ++i) p[i] = 0;
}


// Implements the virt_to_phys() function.
uintptr_t virt_to_phys(uintptr_t va) {
    uint32_t cr3_val;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3_val));

    uint32_t pd_index = va >> 22;          
    uint32_t pt_index = (va >> 12) & 0x3FF;

    uint32_t *pd = (uint32_t*)(cr3_val & 0xFFFFF000); 
    uint32_t pde = pd[pd_index];

    if (!(pde & PAGE_PRESENT)) {
        return (uintptr_t)-1; 
    }

    uint32_t *pt = (uint32_t*)(pde & 0xFFFFF000); 
    uint32_t pte = pt[pt_index];

    if (!(pte & PAGE_PRESENT)) {
        return (uintptr_t)-1; 
    }

    return (pte & 0xFFFFF000) | (va & 0xFFF); 
}

// Switches the active page directory. This function takes a PHYSICAL address.
void paging_switch(uint32_t* directory_phys_addr) {
    current_page_directory_phys = directory_phys_addr;
    __asm__ volatile("mov %0, %%cr3" :: "r"(directory_phys_addr) : "memory");
}

// Allocates and initializes a new page directory.
// Returns a chunk struct containing the PHYSICAL address of the new directory.
struct paging_chunk_4gb * paging_chunk(uint8_t flags) {
    uint32_t *pd_phys = (uint32_t*)alloc_page(); 
    if (pd_phys == NULL) {
        return NULL;
    }
    
    zero_page_phys((uint32_t)pd_phys);

    struct paging_chunk_4gb *chunk = (struct paging_chunk_4gb *)kmalloc(sizeof(struct paging_chunk_4gb));
    if (chunk == NULL) {
        free_page(pd_phys);
        return NULL;
    }
    chunk->directory_entry = pd_phys; 

    return chunk;
}

// Returns the PHYSICAL address of the page directory from the paging_chunk struct.
uint32_t * get_dir_chunk4gb(struct paging_chunk_4gb *chunk) {
    if (chunk == NULL) {
        return NULL;
    }
    return chunk->directory_entry;
}


// Maps a single virtual address to a single physical address with specified flags.
// pd_phys: Physical address of the page directory.
// va: Virtual address to map.
// pa: Physical address to map to.
// flags: Page flags.
int map_page_to(uintptr_t pd_phys, uintptr_t va, uintptr_t pa, uint32_t flags) {
    if ((va % PAGE_SIZE) != 0 || (pa % PAGE_SIZE) != 0) {
        return -EINVARG;
    }

    uint32_t pd_index = va >> 22;           
    uint32_t pt_index = (va >> 12) & 0x3FF; 

    uint32_t *page_directory = (uint32_t*)pd_phys;
    uint32_t pde = page_directory[pd_index];

    uint32_t *page_table;

    if (!(pde & PAGE_PRESENT)) {
        page_table = (uint32_t*)alloc_page();
        if (page_table == NULL) {
            return -ENOMEM;
        }
        zero_page_phys((uint32_t)page_table);

        page_directory[pd_index] = (uint32_t)page_table | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    } else {
        page_table = (uint32_t*)(pde & 0xFFFFF000);
    }

    page_table[pt_index] = (uint32_t)pa | flags;

    return RZOS_ALL_OK;
}

// Maps a range of virtual addresses to a range of physical addresses.
// directory_chunk: The paging_chunk struct.
// virt_start: Starting virtual address.
// phys_start: Starting physical address.
// phys_end: Ending physical address (exclusive).
// flags: Page flags.
int paging_map_to(struct paging_chunk_4gb *directory_chunk, void *virt_start, void *phys_start, void *phys_end, int flags) {
    if(phys_end!=(void*)0x300000){
    if (directory_chunk == NULL || virt_start == NULL || phys_start == NULL || phys_end == NULL) {
        return -EINVARG;
    }
}
else{
    uintptr_t current_virt = (uintptr_t)virt_start;
    uintptr_t current_phys = (uintptr_t)phys_start;
    uintptr_t last_phys = (uintptr_t)phys_end;
    uintptr_t pd_phys_addr = (uintptr_t)directory_chunk->directory_entry;
    while (current_phys < last_phys) {
        int res = map_page_to(pd_phys_addr, current_virt, current_phys, flags);
        if (res != RZOS_ALL_OK) {
            return res; 
        }
        current_virt += PAGE_SIZE;
        current_phys += PAGE_SIZE;
    }
    return RZOS_ALL_OK;

}

    uintptr_t current_virt = (uintptr_t)virt_start;
    uintptr_t current_phys = (uintptr_t)phys_start;
    uintptr_t last_phys = (uintptr_t)phys_end;
    
    if ((current_virt % PAGE_SIZE) != 0 || (current_phys % PAGE_SIZE) != 0) {
        return -EINVARG;
    }

    uintptr_t pd_phys_addr = (uintptr_t)directory_chunk->directory_entry;

    while (current_phys < last_phys) {
        int res = map_page_to(pd_phys_addr, current_virt, current_phys, flags);
        if (res != RZOS_ALL_OK) {
            return res; 
        }
        current_virt += PAGE_SIZE;
        current_phys += PAGE_SIZE;
    }
    return RZOS_ALL_OK;
}

// Maps a single virtual address to a single physical address.
int paging_map(struct paging_chunk_4gb* directory, void* virt, void* phys, int flags)
{
    if (!is_page_aligned(virt) || !is_page_aligned(phys))
    {
        return -EINVARG;
    }

    return paging_set(directory->directory_entry, virt, (uint32_t) phys | flags);
}

// Sets a page table entry to a specific value.
int paging_set(uint32_t* directory_phys_addr, void* virt_add, uint32_t val) {
    uintptr_t va = (uintptr_t)virt_add;
    uintptr_t pa_and_flags = val;

    uint32_t pd_index = va >> 22;
    uint32_t pt_index = (va >> 12) & 0x3FF;

    uint32_t *page_directory = (uint32_t*)(uintptr_t)directory_phys_addr;
    uint32_t pde = page_directory[pd_index];

    uint32_t *page_table;

    if (!(pde & PAGE_PRESENT)) {
        page_table = (uint32_t*)alloc_page(); 
        if (page_table == NULL) {
            return -ENOMEM;
        }
        zero_page_phys((uint32_t)page_table);

        page_directory[pd_index] = (uint32_t)page_table | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    } else {
        page_table = (uint32_t*)(pde & 0xFFFFF000);
    }
    
    page_table[pt_index] = pa_and_flags;
    return RZOS_ALL_OK;
}

// Checks if an address is page-aligned
bool is_page_aligned(void *addr) {
    return ((uintptr_t)addr % PAGE_SIZE) == 0;
}
