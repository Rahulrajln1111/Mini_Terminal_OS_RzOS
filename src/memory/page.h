#ifndef PAGE_H
#define PAGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
/* Page table/directory entry flags */
#define PAGE_PRESENT   0x1
#define PAGE_RW        0x2 //page is writable
#define PAGE_USER      0x4 //access from all
#define PAGE_WTH       0x8 //write through
#define PAGE_CD        0x10 //cache disabled
#define PAGING_TOTAL_ENTRIES_PER_TABLE 0x400 // 1024
#define PAGING_PAGE_SIZE 0X400
#define PAGE_SIZE 0x1000


uintptr_t virt_to_phys(uintptr_t va);
int map_page_to(uintptr_t pd_phys, uintptr_t va, uintptr_t pa, uint32_t flags);
void* map_page_to_virt(uintptr_t pa) ;

void paging_load_dir(uint32_t *dir);

//Struct paging
void paging_switch(uint32_t* directory);
void enable_paging();
uintptr_t create_page_directory(void) ;
struct paging_chunk_4gb
{
	uint32_t *directory_entry;
	
};
struct paging_chunk_4gb * paging_chunk(uint8_t flags);
uint32_t * get_dir_chunk4gb(struct paging_chunk_4gb *chunk);
int paging_set(uint32_t*directory,void*virt_add,uint32_t val);
bool is_page_aligned(void *addr);
int paging_map_to(struct paging_chunk_4gb *directory, void *virt, void *phys, void *phys_end, int flags);

#endif
