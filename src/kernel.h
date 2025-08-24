#include <stddef.h>
#ifndef KERNEL_H
#define KERNEL_H
// #define VGA_WIDTH 80
// #define VGA_HEIGHT 20
void kernel_main();
void print(const char * str);
size_t strlen(const char* str);
#endif
