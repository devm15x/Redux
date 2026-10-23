#ifndef TERMINAL_H
#define TERMINAL_H

#include "limine.h"
#include <stdint.h>

typedef struct {
  int x;
  int y;
  int width;
  int height;
  uint32_t color;
} redux_rect_t;

void terminal_initialize(struct limine_framebuffer *framebuffer);

void terminal_clear(struct limine_framebuffer *framebuffer, uint32_t color);

struct limine_framebuffer *terminal_get_framebuffer(void);

void terminal_set_colors(uint32_t foreground, uint32_t background);

uint32_t terminal_get_foreground(void);
uint32_t terminal_get_background(void);

void terminal_set_cursor(uint32_t x, uint32_t y);

void terminal_cursor_update(void);

void terminal_putchar(char c);

void print(const char *text);
void println(const char *text);
void terminal_putpixel(uint32_t x, uint32_t y, uint32_t color);
void draw_rect(int x, int y, int w, int h, uint32_t color);
void print_uint64(uint64_t value);

#endif
