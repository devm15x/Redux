#include "psf.h"

#include <stdint.h>
#include <stddef.h>
#include "limine.h"

#define PSF1_MAGIC0 0x36
#define PSF1_MAGIC1 0x04
#define PSF1_MODE512 0x01

#define FONT_WIDTH 8
#define FONT_SCALE 2

extern const unsigned char _binary_src_font_psf_start[];
extern const unsigned char _binary_src_font_psf_end[];
extern const unsigned char _binary_src_font_psf_size[];

static struct limine_framebuffer *g_framebuffer = NULL;
static const PSF1_Header *g_font = NULL;

static inline void halt_forever(void)
{
    for (;;)
    {
        __asm__ __volatile__("cli; hlt");
    }
}

void put_pixel(
    uint32_t x,
    uint32_t y,
    uint32_t colour
)
{
    if (g_framebuffer == NULL)
    {
        return;
    }

    if (x >= g_framebuffer->width ||
        y >= g_framebuffer->height)
    {
        return;
    }

    if (g_framebuffer->bpp != 32)
    {
        return;
    }

    volatile uint32_t *pixel =
        (volatile uint32_t *)(
            (uint8_t *)g_framebuffer->address +
            y * g_framebuffer->pitch +
            x * sizeof(uint32_t)
        );

    *pixel = colour;
}

void psf_init(struct limine_framebuffer *framebuffer)
{
    if (framebuffer == NULL)
    {
        halt_forever();
    }

    g_framebuffer = framebuffer;

    g_font =
        (const PSF1_Header *)
        _binary_src_font_psf_start;

    if (g_font->magic[0] != PSF1_MAGIC0 ||
        g_font->magic[1] != PSF1_MAGIC1 ||
        g_font->charsize == 0)
    {
        halt_forever();
    }
}

void psf_draw_char(
    uint8_t character,
    uint32_t cell_x,
    uint32_t cell_y,
    uint32_t foreground,
    uint32_t background
)
{
    if (g_framebuffer == NULL ||
        g_font == NULL)
    {
        return;
    }

    uint32_t glyph_count =
        (g_font->mode & PSF1_MODE512)
            ? 512u
            : 256u;

    uint32_t glyph_index =
        ((uint32_t)character < glyph_count)
            ? (uint32_t)character
            : 0u;

    const uint8_t *glyph =
        _binary_src_font_psf_start +
        sizeof(PSF1_Header) +
        glyph_index * g_font->charsize;

    uint32_t pixel_x =
        cell_x *
        (FONT_WIDTH * FONT_SCALE);

    uint32_t pixel_y =
        cell_y *
        (g_font->charsize * FONT_SCALE);

    for (uint32_t glyph_y = 0;
         glyph_y < g_font->charsize;
         glyph_y++)
    {
        uint8_t row_bits = glyph[glyph_y];

        for (uint32_t glyph_x = 0;
             glyph_x < FONT_WIDTH;
             glyph_x++)
        {
            uint8_t mask =
                (uint8_t)(0x80u >> glyph_x);

            uint32_t colour =
                (row_bits & mask)
                    ? foreground
                    : background;

            for (uint32_t scale_y = 0;
                 scale_y < FONT_SCALE;
                 scale_y++)
            {
                for (uint32_t scale_x = 0;
                     scale_x < FONT_SCALE;
                     scale_x++)
                {
                    put_pixel(
                        pixel_x +
                        glyph_x * FONT_SCALE +
                        scale_x,

                        pixel_y +
                        glyph_y * FONT_SCALE +
                        scale_y,

                        colour
                    );
                }
            }
        }
    }
}

void psf_draw_string(
    const char *text,
    uint32_t start_x,
    uint32_t start_y,
    uint32_t foreground,
    uint32_t background
)
{
    if (text == NULL ||
        g_framebuffer == NULL ||
        g_font == NULL)
    {
        return;
    }

    uint32_t cursor_x = start_x;
    uint32_t cursor_y = start_y;

    while (*text != '\0')
    {
        if (*text == '\n')
        {
            cursor_x = start_x;
            cursor_y++;
        }
        else if (*text == '\r')
        {
            cursor_x = start_x;
        }
        else
        {
            psf_draw_char(
                (uint8_t)*text,
                cursor_x,
                cursor_y,
                foreground,
                background
            );

            cursor_x++;
        }

        text++;
    }
}