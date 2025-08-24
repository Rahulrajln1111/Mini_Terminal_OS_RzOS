/* src/idt/idt.c */
#include <stdint.h>
#include "idt/idt.h"
#include "shell/shell.h"

/* extern ASM stubs for specific ISRs (defined in isr.asm) */
extern void isr14();
extern void isr_common(void); /* if you use a common stub */

/* IDT array and pointer */
static struct idt_entry idt_entries[256];
static struct idt_ptr   idt_ptr;

/* helper to set one gate */
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt_entries[num].base_lo  = (base & 0xFFFF);
    idt_entries[num].base_hi  = (base >> 16) & 0xFFFF;
    idt_entries[num].sel      = sel;
    idt_entries[num].always0  = 0;
    idt_entries[num].flags    = flags;
}

/* public: initialize IDT and load it */
void idt_init(void) {
    /* clear all entries */
    for (int i = 0; i < 256; ++i) {
        idt_set_gate(i, 0, 0x08, 0x00);
    }

    /* Set the page fault gate (interrupt 14) to our stub */
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8E);

    /* Fill idt_ptr and load */
    idt_ptr.limit = (sizeof(struct idt_entry) * 256) - 1;
    idt_ptr.base  = (uint32_t)&idt_entries;

    /* lidt expects memory operand; use inline asm to load */
    __asm__ volatile("lidtl (%0)" : : "r" (&idt_ptr));
}
