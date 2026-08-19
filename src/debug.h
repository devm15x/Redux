#ifndef DEBUG_H
#define DEBUG_H

#include <stdint.h>

static inline void qemu_debug_putc(char c)
{
    __asm__ volatile(
        "outb %0, $0xE9"
        :
        : "a"(c)
    );
}

static inline void qemu_debug_print(const char *text)
{
    while (*text)
    {
        qemu_debug_putc(*text++);
    }
}

/*
 * Print one byte as two hexadecimal characters.
 *
 * Example:
 * 0xEB -> "EB"
 */
static inline void qemu_debug_hex8(uint8_t value)
{
    static const char hex[] =
        "0123456789ABCDEF";

    qemu_debug_putc(
        hex[(value >> 4) & 0xF]
    );

    qemu_debug_putc(
        hex[value & 0xF]
    );
}

#endif