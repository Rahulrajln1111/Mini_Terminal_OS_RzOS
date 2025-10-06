#include <stdint.h>
#include <stddef.h>
#include "memory/page.h"
#include "utils.h"
#include "memory/memory.h" /* alloc_page() should return 4KB-aligned PHYS pointer */
#include "status.h"
uint32_t *cur_dir = 0;

uintptr_t pd_phys_global;
uintptr_t kernel_pd_phys = 0;
/* Exported allocator prototype: we'll cast returned pointer to uint32_t. */
extern void* alloc_page(void);

/* Use the existing serial print function from shell */
extern void print_serial(char *s);


/* Zero 4KiB page at physical address 'phys' */
static void zero_page_phys(uint32_t phys) {
    /* We assume identity-mapped access to physical memory before paging is enabled. */
    uint32_t *p = (uint32_t*)(uintptr_t)phys;
    for (int i = 0; i < 1024; ++i) p[i] = 0;
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


bool is_page_aligned(void *addr){
    return ((uint32_t)addr%PAGE_SIZE)==0;
}

void paging_load_dir(uint32_t *dir);


struct paging_chunk_4gb* paging_chunk(uint8_t flags)
{
    uint32_t * page_dir = kzalloc(sizeof(uint32_t)*PAGING_TOTAL_ENTRIES_PER_TABLE);
    int offset = 0;
    for (int p = 0; p < PAGING_TOTAL_ENTRIES_PER_TABLE; ++p)
        {
            uint32_t *entry = kzalloc(sizeof(uint32_t)*PAGING_TOTAL_ENTRIES_PER_TABLE);
            for (int e = 0; e < PAGING_TOTAL_ENTRIES_PER_TABLE; ++e)
            {
                entry[e] = (offset+(e*PAGING_PAGE_SIZE))|flags;
            }
            offset+=(PAGING_TOTAL_ENTRIES_PER_TABLE*PAGING_PAGE_SIZE);
            page_dir[p] = (uint32_t)entry|flags|PAGE_RW;
        }
        struct paging_chunk_4gb *chunk_4gb = kzalloc(sizeof(struct paging_chunk_4gb));
        chunk_4gb->directory_entry = page_dir;

        return chunk_4gb;    
};



void paging_switch(uint32_t* directory){
    paging_load_dir(directory);
    cur_dir = directory;
} 

uint32_t * get_dir_chunk4gb(struct paging_chunk_4gb *chunk){
    return chunk->directory_entry;
}

int paging_get_index(void*virt_add,uint32_t*directory_idx_out,uint32_t*table_idx_out){

        int res = 0;

        if(!is_page_aligned(virt_add)){
            res = -EINVARG;
            goto out;
        }
    uint32_t addr = (uint32_t)virt_add;
    *directory_idx_out = (addr>>22)&0x3ff;
    *table_idx_out = (addr>>12)&0x3ff;

out:
    return res;
}
int paging_set(uint32_t*directory,void*virt_add,uint32_t val){

    if(!is_page_aligned(virt_add)){
            
            return -EINVARG;
        }

    uint32_t directory_idx = 0;
    uint32_t table_idx = 0;

    int res = paging_get_index(virt_add,&directory_idx,&table_idx);
    if(res<0){
        return res;
    }

    uint32_t entry = directory[directory_idx];
    uint32_t *table = (uint32_t*)(entry&0xfffff000);
    table[table_idx] = val;
    return 0;
}


int paging_map(struct paging_chunk_4gb* directory, void* virt, void* phys, int flags)
{
    if (((unsigned int)virt % PAGING_PAGE_SIZE) || ((unsigned int) phys % PAGING_PAGE_SIZE))
    {
        return -EINVARG;
    }

    return paging_set(directory->directory_entry, virt, (uint32_t) phys | flags);
}

int paging_map_range(struct paging_chunk_4gb* directory, void* virt, void* phys, int count, int flags)
{
    int res = 0;
    for (int i = 0; i < count; i++)
    {
        res = paging_map(directory, virt, phys, flags);
        if (res < 0)
            break;
        virt += PAGING_PAGE_SIZE;
        phys += PAGING_PAGE_SIZE;
    }

    return res;
}

int paging_map_to(struct paging_chunk_4gb *directory, void *virt, void *phys, void *phys_end, int flags)
{
    int res = 0;
    if ((uint32_t)virt % PAGING_PAGE_SIZE)
    {
        res = -EINVARG;
        goto out;
    }
    if ((uint32_t)phys % PAGING_PAGE_SIZE)
    {
        res = -EINVARG;
        goto out;
    }
    if ((uint32_t)phys_end % PAGING_PAGE_SIZE)
    {
        res = -EINVARG;
        goto out;
    }

    if ((uint32_t)phys_end < (uint32_t)phys)
    {
        res = -EINVARG;
        goto out;
    }

    uint32_t total_bytes = phys_end - phys;
    int total_pages = total_bytes / PAGING_PAGE_SIZE;
    res = paging_map_range(directory, virt, phys, total_pages, flags);
out:
    return res;
}





