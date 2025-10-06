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
#define STACK_SIZE 0x2000
#define KERNEL_BASE 0x100000

// Define separate virtual addresses for code and stack
#define USER_CODE_VIRTUAL  0x00010000 // A lower virtual address for code
#define USER_STACK_VIRTUAL 0x40000000 // A higher virtual address for the stack

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


void test_heap_usability(void) {
    kputs("Testing heap usability...\n");
    // 1. Allocate from the heap
    char *test_ptr = (char*)kmalloc(100);
    if (!test_ptr) {
        kputs("FAILURE: kmalloc failed to return a pointer!\n");
        return;
    }
    // 2. Write a unique pattern to the memory
    test_ptr[0] = 'A';
    test_ptr[99] = 'Z';
    // 3. Verify the data
    if (test_ptr[0] == 'A' && test_ptr[99] == 'Z') {
        kputs("SUCCESS: Paging and heap are functional!\n");
    } else {
        kputs("FAILURE: Paging did not correctly map memory for read/write.\n");
    }
    kfree(test_ptr);
}


void user_program_A(void) {
    kputs("Hello from Process A!\n");
    for (;;) {} // Simple loop to halt this process
}

void user_program_B(void) {
    kputs("Hello from Process B!\n");
    for (;;) {} // Simple loop to halt this process
}


void kernel_main(){
    idt_init();
    isr_install();
    memory_init((uint32_t)&kernel_end, 64 * 1024 * 1024);

    uintptr_t idmap_bytes = (uintptr_t)&kernel_end - KERNEL_BASE + STACK_SIZE;
    idmap_bytes = (idmap_bytes + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    next_heap_va = KHEAP_START;
    kheap_init(KHEAP_START);
    page_init();
    test_heap_usability();
    char* test = (char*)kmalloc(100);
    for (int i = 0; i < 100; i++) test[i] = 'A' + (i % 26);
    kfree(test);

print("Hello!\nRazz_--");
print_serial("\nMy name is Razz\n");

terminal_initialize();


    // 1. Create the first process
    pcb_t* proc_a = create_process();
    if (!proc_a) {
        kputs("Failed to create user process A!\n");
        return;
    }

    // 2. Allocate a physical page for the user stack
    uintptr_t stack_a_phys = (uintptr_t)alloc_page();
    if (!stack_a_phys) {
        kputs("Failed to allocate user stack page for A!\n");
        return;
    }
    // Map the physical stack page into the process's virtual space
    // at USER_STACK_VIRTUAL (0x40000000).
    // Use PAGE_RW | PAGE_USER flags to allow user-mode access.
    map_page_to(proc_a->cr3, USER_STACK_VIRTUAL, stack_a_phys, PAGE_RW | PAGE_USER);

    // Set ESP to the very top of the stack page.
    proc_a->esp = USER_STACK_VIRTUAL + PAGE_SIZE - 4;

    // --- CRITICAL CHANGES FOR CODE MAPPING ---
    // 3. Get the physical address of the kernel function user_program_A.
    // This requires a working virt_to_phys function.
    uintptr_t user_program_A_phys = virt_to_phys((uintptr_t)user_program_A);
    if (user_program_A_phys == 0) {
        kputs("Failed to get physical address of user_program_A!\n");
        return;
    }

    // 4. Map this physical code page into the user process's virtual space
    // at a user-space virtual address (USER_CODE_VIRTUAL, e.g., 0x00010000).
    // Use PAGE_PRESENT | PAGE_RW | PAGE_USER flags.
    map_page_to(proc_a->cr3, USER_CODE_VIRTUAL, user_program_A_phys, PAGE_PRESENT | PAGE_RW | PAGE_USER);

    // 5. Set the instruction pointer (EIP) to the user-space virtual address
    // where user_program_A is now mapped.
    proc_a->eip = USER_CODE_VIRTUAL;

    // --- Debugging output before switch ---
    kputs("Switching to Process A...\n");
    kputs("  Process A CR3: "); kputhex(proc_a->cr3); kputs("\n");
    kputs("  Process A EIP: "); kputhex(proc_a->eip); kputs("\n");
    kputs("  Process A ESP: "); kputhex(proc_a->esp); kputs("\n");

    // 6. Perform the context switch
    switch_to_process(proc_a);
}