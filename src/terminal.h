#ifndef TERMINAL_H
#define TERMINAL_H

#include "limine.h"

void terminal_initialize(struct limine_framebuffer *framebuffer);
void terminal_putchar(char c);
void print(const char *text);
void println(const char *text);


#endif