#include <stdint.h>
#include <stddef.h>

#include "gdt.h"

typedef struct
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdtr_t;

static uint64_t gdt[] =
{
    0x0000000000000000,
    0x00AF9A000000FFFF,
    0x00AF92000000FFFF,
    0x00AFFA000000FFFF,
    0x00AFF2000000FFFF
};

static gdtr_t gdtr;

extern void setGdt(
    const gdtr_t *gdtr
);

extern void reloadSegments(void);

void gdt_init(void)
{
    gdtr.limit =
        sizeof(gdt) - 1;

    gdtr.base =
        (uint64_t)(uintptr_t)gdt;

    setGdt(&gdtr);
    reloadSegments();
}