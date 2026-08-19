#include <stdint.h>
#include <libc\string.h>

#define KERNEL_STACK_SIZE 16384

static uint8_t kernel_stack[KERNEL_STACK_SIZE]
    __attribute__((aligned(16)));

struct tss_entry_struct {
    uint32_t reserved0;

    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;

    uint64_t reserved1;

    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;

    uint64_t reserved2;

    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

typedef struct tss_entry_struct tss_entry_t;

static tss_entry_t tss_entry;

struct tss_descriptor {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  limit_high_flags;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed));

typedef struct tss_descriptor tss_descriptor_t;

void write_tss(tss_descriptor_t *g)
{
    uint64_t base =
        (uint64_t)(uintptr_t)&tss_entry;

    uint32_t limit =
        sizeof(tss_entry) - 1;

    memset(g, 0, sizeof(*g));

    g->limit_low =
        limit & 0xFFFF;

    g->base_low =
        base & 0xFFFF;

    g->base_middle =
        (base >> 16) & 0xFF;

    g->access = 0x89;

    g->limit_high_flags =
        (limit >> 16) & 0x0F;

    g->base_high =
        (base >> 24) & 0xFF;

    g->base_upper =
        (base >> 32) & 0xFFFFFFFF;

    g->reserved = 0;
}



void init_tss(void)
{
    memset(&tss_entry, 0, sizeof(tss_entry));

    tss_entry.rsp0 =
        (uint64_t)(uintptr_t)(
            kernel_stack + KERNEL_STACK_SIZE
        );

    tss_entry.iomap_base =
        sizeof(tss_entry);
}

