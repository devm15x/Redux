#include <stdint.h>
#include "panic.h"

#define GDT_OFFSET_KERNEL_CODE 0x28

extern void *isr_stub_table[32];

typedef struct
{
    uint16_t isr_low;
    uint16_t kernel_cs;
    uint8_t  ist;
    uint8_t  attributes;
    uint16_t isr_mid;
    uint32_t isr_high;
    uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

static idt_entry_t idt[256]
    __attribute__((aligned(16)));

typedef struct
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idtr_t;

static idtr_t idtr;

__attribute__((noreturn))
void exception_handler(
    interrupt_frame_t *frame
)
{
    panic_exception(frame);
}

static void idt_set_descriptor(
    uint8_t vector,
    void *isr,
    uint8_t flags
)
{
    idt_entry_t *descriptor =
        &idt[vector];

    uint64_t address =
        (uint64_t)(uintptr_t)isr;

    descriptor->isr_low =
        (uint16_t)(address & 0xFFFF);

    descriptor->kernel_cs =
        GDT_OFFSET_KERNEL_CODE;

    descriptor->ist = 0;

    descriptor->attributes =
        flags;

    descriptor->isr_mid =
        (uint16_t)((address >> 16) & 0xFFFF);

    descriptor->isr_high =
        (uint32_t)((address >> 32) & 0xFFFFFFFF);

    descriptor->reserved = 0;
}

void idt_init(void)
{
    idtr.base =
        (uint64_t)(uintptr_t)&idt[0];

    idtr.limit =
        (uint16_t)(sizeof(idt) - 1);

    for (uint8_t vector = 0;
         vector < 32;
         vector++)
    {
        idt_set_descriptor(
            vector,
            isr_stub_table[vector],
            0x8E
        );
    }

    __asm__ volatile(
        "lidt %0"
        :
        : "m"(idtr)
    );
}