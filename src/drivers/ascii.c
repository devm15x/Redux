#include "ascii.h"
#include <stdint.h>

static const char uk_ascii_map[] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8',  
    '9', '0', '-', '=', '\b','\t', 'q', 'w', 'e', 'r', 
    't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',  0,   
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',  
   '\'', '`',   0, '#', 'z', 'x', 'c', 'v', 'b', 'n',  
    'm', ',', '.', '/',   0, '*',   0, ' ',   0,   0,  
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   
    0,   0,  '7', '8', '9', '-', '4', '5', '6', '+',  
    '1', '2', '3', '0', '.',   0,   0, '\\',  0,   0   
};

char scancode_to_ascii(uint8_t scancode) {

    if (scancode & 0x80) {
        return 0; 
    }
    

    if (scancode >= sizeof(uk_ascii_map)) {
        return 0;
    }
    
    return uk_ascii_map[scancode];
}
