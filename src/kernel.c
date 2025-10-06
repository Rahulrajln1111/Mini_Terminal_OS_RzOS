#include "kernel.h"
#include <stdint.h>
#include <stddef.h>
#include "shell/shell.h"
#include "memory/memory.h"
#include "memory/page.h"
#include "idt/idt.h"
#include "idt/isr.h"
#include "utils.h" 
#include "proc/proc.h"
#include "config.h" 
#include "status.h"

#define kernel_end  0x10a000
#define total_ram_kb 1024*500
#define KERNEL_DIRECT_MAP_OFFSET 0xC0000000 
uint32_t total_physical_bytes = total_ram_kb * 1024;
bool g_is_paging_enabled = false;
uint16_t* video_mem = 0;
uint16_t terminal_row=0;
uint16_t terminal_col=0;
uint32_t* kernel_master_pd_phys = NULL;

size_t strlen(const char* str){
    size_t len=0;
    while(str[len]){
      len++;
    }
    return len;
}

void print(const char * str){
    size_t len = strlen(str);
    for (int i = 0; i < len; ++i)
    {
        terminal_writechar(str[i],15);
    }
}
static struct paging_chunk_4gb * kernel_chunk = 0;

void kernel_main(){
    kheap_init();
    idt_init();
    char *ptr = kzalloc(40);
    ptr[0] = 'E';
    kernel_chunk = paging_chunk(PAGE_USER | PAGE_RW | PAGE_PRESENT);
    if (kernel_chunk == NULL) {
        print_serial("Failed to create kernel paging chunk!\n");
        for(;;); // Halt on error
    }

    if (paging_map_to(kernel_chunk, (void*)0x0, (void*)0x0, (void*)(0x300000), PAGE_USER | PAGE_RW | PAGE_PRESENT) != RZOS_ALL_OK) {
        print_serial("Failed to identity map first 4MB!\n");
        for(;;);
    }
    if (paging_map_to(kernel_chunk,(void*)0xC0000000, (void*)0x300000, (void*)total_physical_bytes, PAGE_PRESENT | PAGE_RW) != RZOS_ALL_OK) {
        print_serial("Failed to map direct physical memory to higher half with offset!\n");
        for(;;);
    }
    

     
    

    paging_switch(get_dir_chunk4gb(kernel_chunk));
    enable_paging();
    g_is_paging_enabled = true;
    kheap_init();

    kputs("Paging enabled and working!\n");
    char *ptr2 = (char*)kzalloc(50);
    for(int i=0;i<40;i++)ptr2[i]=i+'A';

    asm volatile("sti"); // Re-enable interrupts
}
