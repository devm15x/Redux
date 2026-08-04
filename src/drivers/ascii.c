#include "ascii.h"
#include <stdint.h>

static int shift_pressed = 0;

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

char scancode_to_ascii(uint8_t scancode)
{
    if (scancode == 0x2A || scancode == 0x36)
    {
        shift_pressed = 1;
        return 0;
    }

    if (scancode == 0xAA || scancode == 0xB6)
    {
        shift_pressed = 0;
        return 0;
    }

    if (scancode & 0x80)
    {
        return 0;
    }

    if (scancode >= sizeof(uk_ascii_map))
    {
        return 0;
    }

    char c = uk_ascii_map[scancode];

    if (shift_pressed)
    {
        if (c >= 'a' && c <= 'z')
            c -= 32;
        else
        {
            switch (c)
            {
                case '1': c = '!'; break;
                case '2': c = '"'; break;
                case '3': c = '£'; break;
                case '4': c = '$'; break;
                case '5': c = '%'; break;
                case '6': c = '^'; break;
                case '7': c = '&'; break;
                case '8': c = '*'; break;
                case '9': c = '('; break;
                case '0': c = ')'; break;
                case '-': c = '_'; break;
                case '=': c = '+'; break;
                case '[': c = '{'; break;
                case ']': c = '}'; break;
                case ';': c = ':'; break;
                case '\'': c = '@'; break;
                case '#': c = '~'; break;
                case ',': c = '<'; break;
                case '.': c = '>'; break;
                case '/': c = '?'; break;
                case '\\': c = '|'; break;
                case '`': c = '¬'; break;
            }
        }
    }

    return c;
}