#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

uint8_t keyboard_data_available(void);
uint8_t keyboard_get_scancode(void);

#endif
