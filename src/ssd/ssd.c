#include "io/io.h"
#include <stdint.h>

int read_sector(int lba, int total, void *buf) {
    // Select drive + LBA high bits
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    // Sector count
    outb(0x1F2, total);
    // LBA low/mid/high
    outb(0x1F3, (uint8_t)(lba & 0xFF));
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    // Command: READ SECTORS
    outb(0x1F7, 0x20);

    uint16_t *ptr = (uint16_t*) buf;

    for (int i = 0; i < total; i++) {
        // Wait until drive is ready
        uint8_t status;
        do {
            status = insb(0x1F7);
        } while (status & 0x80); //  → still// BSY=1  busy

        while (!(status & 0x08)) { // DRQ=0 → not ready for data
            status = insb(0x1F7);
        }

        // Read one sector = 512 bytes = 256 words
        for (int a = 0; a < 256; a++) {
            *ptr++ = insw(0x1F0);
        }
    }
    return 0;
}
