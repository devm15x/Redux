#include <stdint.h>
#include <stddef.h>
#include "limine.h"

#include "psf.h"

#define FONT_SCALE 2
#define FONT_WIDTH 8
#define FONT_HEIGHT 16

#define CURSOR_BLINK_DELAY 5000000

#define TERMINAL_FOREGROUND 0x00FFFFFF
#define TERMINAL_BACKGROUND 0x00081A33

static struct limine_framebuffer *terminal_framebuffer = NULL;

static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;

static uint32_t terminal_width = 0;
static uint32_t terminal_height = 0;

static uint32_t cursor_counter = 0;
static uint8_t cursor_visible = 0;

static void terminal_draw_cursor(void);
static void terminal_erase_cursor(void);
static void terminal_scroll(void);
static void terminal_check_scroll(void);

void terminal_initialize(struct limine_framebuffer *framebuffer)
{
    terminal_framebuffer = framebuffer;

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
    terminal_framebuffer = framebuffer;

    for (uint32_t y = 0; y < framebuffer->height; y++)
    {
        for (uint32_t x = 0; x < framebuffer->width; x++)
        {
            put_pixel(x, y, TERMINAL_BACKGROUND);
        }
    }

    cursor_x = 1;
    cursor_y = 1;

    cursor_counter = 0;
    cursor_visible = 0;
}

static void terminal_draw_cursor(void)
{
    psf_draw_char(
        219,
        cursor_x,
        cursor_y,
        TERMINAL_FOREGROUND,
        TERMINAL_BACKGROUND
    );

    cursor_visible = 1;
}

static void terminal_erase_cursor(void)
{
    psf_draw_char(
        ' ',
        cursor_x,
        cursor_y,
        TERMINAL_FOREGROUND,
        TERMINAL_BACKGROUND
    );

    cursor_visible = 0;
}

/*
 * Move the framebuffer upward by one character row.
 *
 * One terminal row occupies:
 *
 *     FONT_HEIGHT * FONT_SCALE
 *
 * physical pixel rows.
 */
static void terminal_scroll(void)
{
    if (terminal_framebuffer == NULL)
    {
        return;
    }

    uint8_t *framebuffer_memory =
        (uint8_t *)terminal_framebuffer->address;

    uint64_t pitch =
        terminal_framebuffer->pitch;

    uint32_t bytes_per_pixel =
        terminal_framebuffer->bpp / 8;

    uint32_t pixel_rows_to_move =
        FONT_HEIGHT * FONT_SCALE;

    uint64_t byte_offset =
        (uint64_t)pixel_rows_to_move * pitch;

    uint64_t bytes_to_copy =
        (terminal_framebuffer->height - pixel_rows_to_move) *
        pitch;

    /*
     * Copy upward.
     *
     * The destination is below the source in memory, so copying
     * forwards is safe even though the regions overlap.
     */
    for (uint64_t i = 0; i < bytes_to_copy; i++)
    {
        framebuffer_memory[i] =
            framebuffer_memory[i + byte_offset];
    }

    /*
     * Clear the newly exposed pixel rows at the bottom.
     */
    uint32_t clear_start_y =
        terminal_framebuffer->height -
        pixel_rows_to_move;

    for (uint32_t y = clear_start_y;
         y < terminal_framebuffer->height;
         y++)
    {
        uint8_t *row =
            framebuffer_memory +
            ((uint64_t)y * pitch);

        for (uint32_t x = 0;
             x < terminal_framebuffer->width;
             x++)
        {
            uint8_t *pixel =
                row + ((uint64_t)x * bytes_per_pixel);

            /*
             * Limine normally gives us BGR/XRGB-style framebuffer
             * storage. Copy the background colour byte by byte.
             */
            if (bytes_per_pixel >= 1)
            {
                pixel[0] =
                    (uint8_t)(TERMINAL_BACKGROUND & 0xFF);
            }

            if (bytes_per_pixel >= 2)
            {
                pixel[1] =
                    (uint8_t)(
                        (TERMINAL_BACKGROUND >> 8) & 0xFF
                    );
            }

            if (bytes_per_pixel >= 3)
            {
                pixel[2] =
                    (uint8_t)(
                        (TERMINAL_BACKGROUND >> 16) & 0xFF
                    );
            }

            if (bytes_per_pixel >= 4)
            {
                pixel[3] = 0;
            }
        }
    }
}

static void terminal_check_scroll(void)
{
    if (cursor_y < terminal_height)
    {
        return;
    }

    terminal_scroll();

    cursor_y = terminal_height - 1;
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

        terminal_check_scroll();
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
            TERMINAL_FOREGROUND,
            TERMINAL_BACKGROUND
        );

        terminal_draw_cursor();
        return;
    }

    psf_draw_char(
        (uint8_t)c,
        cursor_x,
        cursor_y,
        TERMINAL_FOREGROUND,
        TERMINAL_BACKGROUND
    );

    cursor_x++;

    if (cursor_x >= terminal_width)
    {
        cursor_x = 1;
        cursor_y++;
    }

    terminal_check_scroll();
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

void print_uint64(uint64_t value)
{
    char buffer[21];
    size_t position = sizeof(buffer) - 1;

    buffer[position] = '\0';

    if (value == 0)
    {
        print("0");
        return;
    }

    while (value > 0)
    {
        position--;

        buffer[position] =
            (char)('0' + (value % 10));

        value /= 10;
    }

    print(&buffer[position]);
}