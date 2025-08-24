#ifndef PROC_H
#define PROC_H

#include <stdint.h>
#include <stddef.h>

// ... pcb_t struct
typedef struct {
    uint32_t esp, ebp;     // Stack pointers
    uint32_t eip;          // Instruction pointer
    uint32_t cr3;          // Physical address of the page directory
    uint32_t pid;          // Process ID
    // Add other general-purpose registers as needed (eax, ebx, etc.)
} pcb_t;

// Declare the global variables using 'extern'
#define MAX_PROCESSES 10
extern pcb_t process_table[MAX_PROCESSES];
extern uint32_t next_pid;
extern pcb_t* current_process;

pcb_t* create_process();
void switch_to_process(pcb_t* next_pcb);
#endif // PROC_H