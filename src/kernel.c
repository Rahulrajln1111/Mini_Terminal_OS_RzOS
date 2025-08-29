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

extern uint32_t kernel_end;
uint16_t* video_mem = 0;
uint16_t terminal_row=0;
uint16_t terminal_col=0;


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



void user_program_A(void) {
    kputs("Hello from Process A!\n");
    for (;;) {} // Simple loop to halt this process
}

void user_program_B(void) {
    kputs("Hello from Process B!\n");
    for (;;) {} // Simple loop to halt this process
}

void ensure_low_memory_identity_mapped(struct paging_chunk_4gb *chunk) {
    // map virtual 0x0..0x400000 to physical 0x0..0x400000
    // PAGING_PAGE_SIZE = 4096
    // flags: present + writable + supervisor (no USER)
    int flags = PAGE_PRESENT | PAGE_RW; // don't need PAGE_USER for kernel pages
    paging_map_to(chunk,
                  (void*)0x00000000,            // virt start
                  (void*)0x00000000,            // phys start
                  (void*)0x00400000,            // phys_end (exclusive)
                  flags);
}

static struct paging_chunk_4gb * kernel_chunk = 0;

void kernel_main(){
    kheap_init();
    idt_init();
    kernel_chunk = paging_chunk(PAGE_USER|PAGE_RW|PAGE_PRESENT);
    
    char *ptr = kzalloc(4096);

    //paging_set(get_dir_chunk4gb(kernel_chunk),(void*)0x1000,(uint32_t)ptr|PAGE_PRESENT|PAGE_RW|PAGE_USER);
    paging_switch(get_dir_chunk4gb(kernel_chunk));
    enable_paging();

    for (int i = 0; i < 100; i++) ptr[i] = 'A' + (i % 26);
    char *ptr2 = (char*)0x1010;
    kputs(ptr2);
    kputs(ptr);

print("Hello!\nRazz_--");
print_serial("\nMy name is Razz\n");

terminal_initialize();

}


