#include "io.h"
#include "keyboard.h"
//checks if there is any data available from the keyboard
uint8_t keyboard_data_available(void) {
     return (inb(0x64) & 0x01);
}

uint8_t keyboard_get_scancode(void) {
    if (keyboard_data_available()) {
        io_wait(); 
        return inb(0x60); 
    }
    return 0;
}