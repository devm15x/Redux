#ifndef ASCII_H
#define ASCII_H

#include <stdint.h>

// Translates a Scan Code Set 1 byte into a printable ASCII character
char scancode_to_ascii(uint8_t scancode);

#endif
