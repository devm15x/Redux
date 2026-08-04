#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include "psf.h"
#include "terminal.h"
#include "drivers\ascii.h"
#include "drivers\keyboard.h"
//here are some attributes, now go have fun and leave me alone

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = { .id = LIMINE_FRAMEBUFFER_REQUEST_ID, .revision = 0};

__attribute__((used , section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used , section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

void *memcpy(void *restrict dest, const void *restrict src, size_t n) {
    uint8_t *restrict pdest = dest;
    const uint8_t *restrict psrc = src;

    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }

    return dest;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = s;

    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }

    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = dest;
    const uint8_t *psrc = src;

    if ((uintptr_t)src > (uintptr_t)dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if ((uintptr_t)src < (uintptr_t)dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }

}

static void halt(void) {
    for (;;) {
        asm("hlt");
    }
}


void kmain(void) {
    if(LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        halt();
    }

    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        halt();
    }

struct limine_framebuffer *framebuffer =
    framebuffer_request.response->framebuffers[0];
    terminal_initialize(framebuffer);

    /* Must happen before put_pixel(). */
    psf_init(framebuffer);

    /* Now this can access g_framebuffer. */
    for (uint32_t y = 0; y < framebuffer->height; y++)
    {
        for (uint32_t x = 0; x < framebuffer->width; x++)
        {
            put_pixel(x, y, 0x081A33);
        }
    }
        println("Redux Kernel v0.0.1");
        terminal_putchar('\n');
        println("Copyright (C) 2026-present devm15");
        println("Licensed under the MIT License.");
        while (1) {
            uint8_t scancode = keyboard_get_scancode();
            if (scancode != 0) {
                char c = scancode_to_ascii(scancode);
                if (c != 0) {
                    terminal_putchar(c);
                }   
            }
        }
        halt();
}