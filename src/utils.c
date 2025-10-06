// utils.c
#include <stdint.h>
#include <stddef.h>

#include "shell/shell.h"
#include "utils.h"
#include "kernel.h"
void int_to_hex(uint32_t n, char* out) {
    const char* hex = "0123456789ABCDEF";
    for (int i = 7; i >= 0; i--) {
        out[i] = hex[n & 0xF];
        n >>= 4;
    }
    out[8] = '\0';
}

void kputs(const char* s) {
    size_t len = strlen(s);
    for (size_t i = 0; i < len; i++) {
        serial_write(s[i]);
    }
}

void kputhex(uint32_t x) {
    char b[11];
    static const char hex[] = "0123456789ABCDEF";
    b[0] = '0'; b[1] = 'x';
    for (int i = 0; i < 8; ++i) {
        uint32_t shift = (7 - i) * 4;
        b[2 + i] = hex[(x >> shift) & 0xF];
    }
    b[10] = '\0';
    print_serial(b);
}

