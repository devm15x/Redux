#include <stdint.h>
#include "limine.h"

#include "psf.h"

#define FONT_SCALE 2
#define FONT_WIDTH 8
#define FONT_HEIGHT 16

#define CURSOR_BLINK_DELAY 5000000

static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;

static uint32_t terminal_width = 0;
static uint32_t terminal_height = 0;

static uint32_t cursor_counter = 0;
static uint8_t cursor_visible = 0;

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

    cursor_counter = 0;
    cursor_visible = 0;
}
void terminal_clear(struct limine_framebuffer *framebuffer)
{
    for (uint32_t y = 0; y < framebuffer->height; y++)
    {
        for (uint32_t x = 0; x < framebuffer->width; x++)
        {
            put_pixel(x, y, 0x00081A33);
        }
    }

    cursor_x = 1;
    cursor_y = 1;
}

static void terminal_draw_cursor(void)
{
    psf_draw_char(
        219,
        cursor_x,
        cursor_y,
        0x00FFFFFF,
        0x00081A33
    );

    cursor_visible = 1;
}

static void terminal_erase_cursor(void)
{
    psf_draw_char(
        ' ',
        cursor_x,
        cursor_y,
        0x00FFFFFF,
        0x00081A33
    );

    cursor_visible = 0;
}

void terminal_cursor_update(void)
{
    cursor_counter++;

    if (cursor_counter < CURSOR_BLINK_DELAY)
    {
        return;
    }

    cursor_counter = 0;

    if (cursor_visible)
    {
        terminal_erase_cursor();
    }
    else
    {
        terminal_draw_cursor();
    }
}

void terminal_putchar(char c)
{
    if (cursor_visible)
    {
        terminal_erase_cursor();
    }

    cursor_counter = 0;

    if (c == '\n')
    {
        cursor_x = 1;
        cursor_y++;

        if (cursor_y >= terminal_height)
        {
            cursor_y = terminal_height - 1;
        }

        terminal_draw_cursor();
        return;
    }

    if (c == '\b')
    {
        if (cursor_x > 1)
        {
            cursor_x--;
        }
        else if (cursor_y > 1)
        {
            cursor_y--;
            cursor_x = terminal_width - 1;
        }

        psf_draw_char(
            ' ',
            cursor_x,
            cursor_y,
            0x00FFFFFF,
            0x00081A33
        );

        terminal_draw_cursor();
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

    terminal_draw_cursor();
}

void print(const char *text)
{
    while (*text)
    {
        terminal_putchar(*text++);
    }
}

void println(const char *text)
{
    print(text);
    terminal_putchar('\n');
}