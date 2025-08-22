#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

void terminal_initialize();
void terminal_putchar(int x,int y,char c,uint16_t colour);
void print_serial(const char* str);
void serial_write(char c);
void terminal_writechar(char c,char colour);

uint16_t terminal_make_char(char c,char colour);
#endif
