#include <stdint.h>
#include "limine.h"

#include "psf.h"

#define FONT_SCALE 2
#define FONT_WIDTH 8
#define FONT_HEIGHT 16

static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;

static uint32_t terminal_width = 0;
static uint32_t terminal_height = 0;

void terminal_initialize(struct limine_framebuffer *framebuffer)
{
    terminal_width =
        framebuffer->width /
        (FONT_WIDTH * FONT_SCALE);

    terminal_height =
        framebuffer->height /
        (FONT_HEIGHT * FONT_SCALE);

    cursor_x = 1;
    cursor_y = 1;
}

void terminal_putchar(char c)
{
    if (c == '\n')
    {
        cursor_x = 1;
        cursor_y++;
        return;
    }

    psf_draw_char(
        (uint8_t)c,
        cursor_x,
        cursor_y,
        0x00FFFFFF,
        0x00081A33
    );

    cursor_x++;

    if (cursor_x >= terminal_width)
    {
        cursor_x = 1;
        cursor_y++;
    }

    if (cursor_y >= terminal_height)
    {
        cursor_y = terminal_height - 1;
    }
}

void print(const char *text)
{
    while (*text)
    {
        terminal_putchar(*text++);
    }
}
void println(const char *text) {
    print(text);
    terminal_putchar('\n');
}