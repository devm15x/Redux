#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>

#include "psf.h"
#include "terminal.h"
#include "drivers/ascii.h"
#include "drivers/keyboard.h"
#include "shell.h"

//here are some attributes, now go have fun and leave me alone
__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] =
    LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] =
    LIMINE_BASE_REVISION(6);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] =
    LIMINE_REQUESTS_END_MARKER;

void *memcpy(void *restrict dest, const void *restrict src, size_t n)
{
    uint8_t *restrict pdest = dest;
    const uint8_t *restrict psrc = src;

    for (size_t i = 0; i < n; i++)
    {
        pdest[i] = psrc[i];
    }

    return dest;
}

void *memset(void *s, int c, size_t n)
{
    uint8_t *p = s;

    for (size_t i = 0; i < n; i++)
    {
        p[i] = (uint8_t)c;
    }

    return s;
}

void *memmove(void *dest, const void *src, size_t n)
{
    uint8_t *pdest = dest;
    const uint8_t *psrc = src;

    if ((uintptr_t)src > (uintptr_t)dest)
    {
        for (size_t i = 0; i < n; i++)
        {
            pdest[i] = psrc[i];
        }
    }
    else if ((uintptr_t)src < (uintptr_t)dest)
    {
        for (size_t i = n; i > 0; i--)
        {
            pdest[i - 1] = psrc[i - 1];
        }
    }

    return dest;
}

static void print_uint64(uint64_t value)
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
        buffer[position] = (char)('0' + (value % 10));
        value /= 10;
    }

    print(&buffer[position]);
}

static uint64_t get_usable_ram_kb(void)
{
    if (memmap_request.response == NULL)
    {
        return 0;
    }

    uint64_t usable_bytes = 0;

    for (uint64_t i = 0;
         i < memmap_request.response->entry_count;
         i++)
    {
        struct limine_memmap_entry *entry =
            memmap_request.response->entries[i];

        if (entry->type == LIMINE_MEMMAP_USABLE)
        {
            usable_bytes += entry->length;
        }
    }

    return usable_bytes / 1024;
}

static void halt(void)
{
    for (;;)
    {
        asm volatile("hlt");
    }
}

void kmain(void)
{
    if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision))
    {
        halt();
    }

    if (framebuffer_request.response == NULL ||
        framebuffer_request.response->framebuffer_count < 1)
    {
        halt();
    }

    struct limine_framebuffer *framebuffer =
        framebuffer_request.response->framebuffers[0];

    terminal_initialize(framebuffer);
    psf_init(framebuffer);
    terminal_clear(framebuffer);

    println("Redux Kernel v0.0.1");
    terminal_putchar('\n');

    println("Copyright (C) 2026-present devm15");
    println("Licensed under the MIT License.");

    print("Available Memory: ");
    print_uint64(get_usable_ram_kb());
    println(" KB");

    shell_init(framebuffer);

    while (1)
    {
        shell_update();
    }
}