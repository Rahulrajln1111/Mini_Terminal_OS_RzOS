#include "shell.h"
#include "io/io.h"
#include <stddef.h>


static uint16_t* video_mem;
static uint16_t terminal_row = 0;
static uint16_t terminal_col = 0;

void terminal_writechar(char c,char colour){
 if(c=='\n'){
     terminal_col=0;
     terminal_row+=1;
     return;
 }
}

void serial_write(char c){
    // wait until transmit holding register is empty
    while ((insb(0x3F8 + 5) & 0x20) == 0);
    outb(0x3F8, c);
}

void print_serial(const char* str){
    for (size_t i = 0; str[i]; i++)
        serial_write(str[i]);
}
uint16_t terminal_make_char(char c,char colour){
    return (colour<<8 | c);
}

void terminal_putchar(int x,int y,char c,uint16_t colour){
    video_mem[(y*VGA_WIDTH+x)]=terminal_make_char(c,colour);
}

void terminal_initialize() {
    print_serial("\nNow you are inside termina\n");
    print_serial("\nRazz-#");
    video_mem = (uint16_t*)0xB8000;
    for (int y = 0; y < VGA_HEIGHT; y++)
        for (int x = 0; x < VGA_WIDTH; x++)
            terminal_putchar(x, y, ' ', 0);


}
