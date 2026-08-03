#ifndef PSF_H
#define PSF_H

#include <stdint.h>
#include "limine.h"

typedef struct
{
    uint8_t magic[2];
    uint8_t mode;
    uint8_t charsize;
} PSF1_Header;

void psf_init(
    struct limine_framebuffer *framebuffer
);

void psf_draw_char(
    uint8_t character,
    uint32_t cell_x,
    uint32_t cell_y,
    uint32_t foreground,
    uint32_t background
);

void psf_draw_string(
    const char *text,
    uint32_t start_x,
    uint32_t start_y,
    uint32_t foreground,
    uint32_t background
);
void put_pixel(uint32_t x, uint32_t y, uint32_t colour);

#endif