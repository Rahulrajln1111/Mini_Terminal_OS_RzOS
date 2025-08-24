// In src/proc/proc.c
#include "proc/proc.h"
#include "memory/page.h" // Assuming you have map_page and create_page_directory

pcb_t process_table[MAX_PROCESSES];
uint32_t next_pid = 0;
pcb_t* current_process = NULL;

// Function to create a new process
pcb_t* create_process() {
    if (next_pid >= MAX_PROCESSES) {
        return NULL; // No more room in the process table
    }

    pcb_t* pcb = &process_table[next_pid];
    pcb->pid = next_pid++;

    // 1. Create a new page directory for the process
    pcb->cr3 = create_page_directory(); // This function needs to be implemented

    // 2. Set up a simple initial state for the process
    pcb->eip = 0x0; // Placeholder for the program entry point
    pcb->esp = 0x0; // Placeholder for the user stack pointer

    return pcb;
}

// The core context switching function
void switch_to_process(pcb_t* next_pcb) {
    if (next_pcb == NULL) return;

    // Save the current process's state before switching
    if (current_process != NULL) {
        __asm__ volatile("mov %%esp, %0" : "=r"(current_process->esp));
        __asm__ volatile("mov %%ebp, %0" : "=r"(current_process->ebp));
        // Note: EIP is saved by the 'ret' instruction in the caller's stack frame
    }

    current_process = next_pcb;

    // Load the new process's page directory into CR3
    __asm__ volatile("mov %0, %%cr3" :: "r"(current_process->cr3));

    // Restore the new process's stack
    __asm__ volatile("mov %0, %%esp" :: "r"(current_process->esp));
    __asm__ volatile("mov %0, %%ebp" :: "r"(current_process->ebp));

    // Restore the instruction pointer to jump to the new process
    __asm__ volatile("jmp *%0" :: "r"(current_process->eip));
}