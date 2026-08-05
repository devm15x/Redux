#ifndef TERMINAL_H
#define TERMINAL_H

#include "limine.h"

void terminal_initialize(struct limine_framebuffer *framebuffer);
void terminal_putchar(char c);
void terminal_cursor_update(void);
void print(const char *text);
void println(const char *text);
void terminal_clear(struct limine_framebuffer *framebuffer);
void print_uint64(uint64_t value);

#endif