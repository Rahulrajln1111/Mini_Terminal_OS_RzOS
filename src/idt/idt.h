#ifndef IDT_H
#define IDT_H
#include <stdint.h>
struct idt_desc
{
	uint16_t offset_1;//offset bits0-15;
	uint16_t selector;//selectot thats in out GDT
	uint8_t zero;
	uint8_t type_attr;
	uint16_t offset_2;

}__attribute__((packed));

struct idtr_desc
{
	uint16_t limit;//size of descriptor table -1;
	uint32_t base;//base address of the start of hte interupt descriptor table 
}__attribute__((packed));

void idt_init();
#endif