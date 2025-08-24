#include "kernel.h"
#include<stdint.h>
#include<stddef.h>
#include "shell/shell.h"
#include "memory/memory.h"
#include "memory/page.h"
#include "idt/idt.h"
#include "idt/isr.h"
#include "utils.h"

extern uint32_t kernel_end;
uint16_t* video_mem = 0;
uint16_t terminal_row=0;
uint16_t terminal_col=0;
#define STACK_SIZE 0x2000
#define KERNEL_BASE 0x100000

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

void kernel_main(){
	idt_init();
    isr_install();
	memory_init((uint32_t)&kernel_end, 64 * 1024 * 1024);

	uintptr_t idmap_bytes = (uintptr_t)&kernel_end - KERNEL_BASE + STACK_SIZE;
    idmap_bytes = (idmap_bytes + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    page_init();


	next_heap_va = KHEAP_START; 
    kheap_init(KHEAP_START);

    char* test = (char*)kmalloc(100);
    for (int i = 0; i < 100; i++) test[i] = 'A' + (i % 26);
    kfree(test);

print("Hello!\nRazz_--");
print_serial("\nMy name is Razz\n");
terminal_initialize();
//Initialize the interrupt descriptor table;
// idt_init();
// outb(0x60,0xff);
}