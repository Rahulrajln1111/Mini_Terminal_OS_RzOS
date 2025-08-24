// utils.c
#include <stdint.h>

void int_to_hex(uint32_t n, char* out) {
    const char* hex = "0123456789ABCDEF";
    for (int i = 7; i >= 0; i--) {
        out[i] = hex[n & 0xF];
        n >>= 4;
    }
    out[8] = '\0';
}
