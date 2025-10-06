#include "kernel.h"
#include<stdint.h>
#include<stddef.h>
#include "shell/shell.h"
#include "memory/memory.h"

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

void kernel_main(){
	memory_init((uint32_t)&kernel_end, 64 * 1024 * 1024);
    kheap_init(kernel_end);

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