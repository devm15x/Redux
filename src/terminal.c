#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
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

/*
 * Keep track of the currently active terminal colours.
 *
 * This means a panic screen can switch the background to red
 * and cursor drawing / scrolling will continue using red.
 */
static uint32_t current_foreground =
    TERMINAL_FOREGROUND;

static uint32_t current_background =
    TERMINAL_BACKGROUND;


static void terminal_draw_cursor(void);
static void terminal_erase_cursor(void);
static void terminal_scroll(void);
static void terminal_check_scroll(void);


/* ============================================================
 * Initialization
 * ============================================================
 */

void terminal_initialize(
    struct limine_framebuffer *framebuffer
)
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

    current_foreground =
        TERMINAL_FOREGROUND;

    current_background =
        TERMINAL_BACKGROUND;
}


/* ============================================================
 * Framebuffer access
 * ============================================================
 */

struct limine_framebuffer *
terminal_get_framebuffer(void)
{
    return terminal_framebuffer;
}


/* ============================================================
 * Colours
 * ============================================================
 */

void terminal_set_colors(
    uint32_t foreground,
    uint32_t background
)
{
    current_foreground =
        foreground;

    current_background =
        background;
}

uint32_t terminal_get_foreground(void)
{
    return current_foreground;
}

uint32_t terminal_get_background(void)
{
    return current_background;
}


/* ============================================================
 * Clear
 * ============================================================
 */

void terminal_clear(
    struct limine_framebuffer *framebuffer,
    uint32_t color
)
{
    if (framebuffer == NULL)
    {
        return;
    }

    terminal_framebuffer =
        framebuffer;

    /*
     * Make the requested clear colour the new active
     * terminal background.
     */
    current_background =
        color;

    for (uint32_t y = 0;
         y < framebuffer->height;
         y++)
    {
        for (uint32_t x = 0;
             x < framebuffer->width;
             x++)
        {
            put_pixel(
                x,
                y,
                color
            );
        }
    }

    cursor_x = 1;
    cursor_y = 1;

    cursor_counter = 0;
    cursor_visible = 0;
}


/* ============================================================
 * Cursor
 * ============================================================
 */

static void terminal_draw_cursor(void)
{
    if (terminal_framebuffer == NULL)
    {
        return;
    }

    psf_draw_char(
        219,
        cursor_x,
        cursor_y,
        current_foreground,
        current_background
    );

    cursor_visible = 1;
}

static void terminal_erase_cursor(void)
{
    if (terminal_framebuffer == NULL)
    {
        return;
    }

    psf_draw_char(
        ' ',
        cursor_x,
        cursor_y,
        current_foreground,
        current_background
    );

    cursor_visible = 0;
}

extern volatile uint64_t redux_system_ticks;

static uint64_t cursor_last_tick = 0;

void terminal_cursor_update(void)
{
    if (redux_system_ticks == cursor_last_tick)
    {
        return;
    }

    cursor_last_tick =
        redux_system_ticks;

    if (cursor_visible)
    {
        terminal_erase_cursor();
        cursor_visible = false;
    }
    else
    {
        terminal_draw_cursor();
        cursor_visible = true;
    }
}

void terminal_set_cursor(
    uint32_t x,
    uint32_t y
)
{
    if (terminal_width == 0 ||
        terminal_height == 0)
    {
        return;
    }

    if (cursor_visible)
    {
        terminal_erase_cursor();
    }

    /*
     * Your terminal intentionally starts at cell 1,
     * leaving a small border around the display.
     */
    if (x < 1)
    {
        x = 1;
    }

    if (y < 1)
    {
        y = 1;
    }

    if (x >= terminal_width)
    {
        x = terminal_width - 1;
    }

    if (y >= terminal_height)
    {
        y = terminal_height - 1;
    }

    cursor_x = x;
    cursor_y = y;

    cursor_counter = 0;

    terminal_draw_cursor();
}


/* ============================================================
 * Scrolling
 * ============================================================
 */

static void terminal_scroll(void)
{
    if (terminal_framebuffer == NULL)
    {
        return;
    }

    uint8_t *framebuffer_memory =
        (uint8_t *)
        terminal_framebuffer->address;

    uint64_t pitch =
        terminal_framebuffer->pitch;

    uint32_t bytes_per_pixel =
        terminal_framebuffer->bpp / 8;

    uint32_t pixel_rows_to_move =
        FONT_HEIGHT * FONT_SCALE;

    if (pixel_rows_to_move >=
        terminal_framebuffer->height)
    {
        return;
    }

    uint64_t byte_offset =
        (uint64_t)
        pixel_rows_to_move *
        pitch;

    uint64_t bytes_to_copy =
        (
            terminal_framebuffer->height -
            pixel_rows_to_move
        ) *
        pitch;

    /*
     * Copy framebuffer upward by one text row.
     */
    for (uint64_t i = 0;
         i < bytes_to_copy;
         i++)
    {
        framebuffer_memory[i] =
            framebuffer_memory[
                i + byte_offset
            ];
    }

    /*
     * Clear the newly exposed bottom rows using
     * the CURRENT background colour.
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
             x <
             terminal_framebuffer->width;
             x++)
        {
            uint8_t *pixel =
                row +
                (
                    (uint64_t)x *
                    bytes_per_pixel
                );

            if (bytes_per_pixel >= 1)
            {
                pixel[0] =
                    (uint8_t)(
                        current_background &
                        0xFF
                    );
            }

            if (bytes_per_pixel >= 2)
            {
                pixel[1] =
                    (uint8_t)(
                        (
                            current_background >>
                            8
                        ) &
                        0xFF
                    );
            }

            if (bytes_per_pixel >= 3)
            {
                pixel[2] =
                    (uint8_t)(
                        (
                            current_background >>
                            16
                        ) &
                        0xFF
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
    if (cursor_y <
        terminal_height)
    {
        return;
    }

    terminal_scroll();

    cursor_y =
        terminal_height - 1;
}


/* ============================================================
 * Character output
 * ============================================================
 */

void terminal_putchar(char c)
{
    if (terminal_framebuffer == NULL)
    {
        return;
    }

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

            cursor_x =
                terminal_width - 1;
        }

        psf_draw_char(
            ' ',
            cursor_x,
            cursor_y,
            current_foreground,
            current_background
        );

        terminal_draw_cursor();

        return;
    }

    psf_draw_char(
        (uint8_t)c,
        cursor_x,
        cursor_y,
        current_foreground,
        current_background
    );

    cursor_x++;

    if (cursor_x >=
        terminal_width)
    {
        cursor_x = 1;
        cursor_y++;
    }

    terminal_check_scroll();
    terminal_draw_cursor();
}


/* ============================================================
 * Printing
 * ============================================================
 */

void print(const char *text)
{
    if (text == NULL)
    {
        return;
    }

    while (*text)
    {
        terminal_putchar(
            *text++
        );
    }
}

void println(const char *text)
{
    print(text);
    terminal_putchar('\n');
}

void print_uint64(
    uint64_t value
)
{
    char buffer[21];

    size_t position =
        sizeof(buffer) - 1;

    buffer[position] =
        '\0';

    if (value == 0)
    {
        print("0");
        return;
    }

    while (value > 0)
    {
        position--;

        buffer[position] =
            (char)(
                '0' +
                (value % 10)
            );

        value /= 10;
    }

    print(
        &buffer[position]
    );
}