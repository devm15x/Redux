#include <stdint.h>
#include <stddef.h>

#include "gdt.h"
#include "tss.h"

struct gdt_entry_bits {
    unsigned int limit_low              : 16;
    unsigned int base_low               : 24;
    unsigned int accessed               : 1;
    unsigned int read_write             : 1;
    unsigned int conforming_expand_down : 1;
    unsigned int code                   : 1;
    unsigned int code_data_segment      : 1;
    unsigned int DPL                    : 2;
    unsigned int present                : 1;
    unsigned int limit_high             : 4;
    unsigned int available              : 1;
    unsigned int long_mode              : 1;
    unsigned int big                    : 1;
    unsigned int gran                   : 1;
    unsigned int base_high              : 8;
} __attribute__((packed));

typedef struct gdt_entry_bits gdt_entry_bits;

typedef struct
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdtr_t;



static gdt_entry_bits gdt[7];

gdt_entry_bits *ring0_code = &gdt[1];
gdt_entry_bits *ring0_data = &gdt[2];

gdt_entry_bits *ring3_data = &gdt[3];
gdt_entry_bits *ring3_code = &gdt[4];




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

    ring0_code->limit_low = 0xFFFF;
    ring0_code->base_low = 0;

    ring0_code->accessed = 0;
    ring0_code->read_write = 1;
    ring0_code->conforming_expand_down = 0;

    ring0_code->code = 1;
    ring0_code->code_data_segment = 1;

    ring0_code->DPL = 0;
    ring0_code->present = 1;

    ring0_code->limit_high = 0xF;
    ring0_code->available = 0;

    ring0_code->long_mode = 1;
    ring0_code->big = 0;

    ring0_code->gran = 1;
    ring0_code->base_high = 0;


    *ring0_data = *ring0_code;

    ring0_data->code = 0;
    ring0_data->long_mode = 0;
    ring0_data->big = 1;
    ring3_code->limit_low = 0xFFFF;
    ring3_code->base_low = 0;

    ring3_code->accessed = 0;
    ring3_code->read_write = 1;
    ring3_code->conforming_expand_down = 0;

    ring3_code->code = 1;
    ring3_code->code_data_segment = 1;

    ring3_code->DPL = 3;
    ring3_code->present = 1;

    ring3_code->limit_high = 0xF;
    ring3_code->available = 0;

    ring3_code->long_mode = 1;
    ring3_code->big = 0;

    ring3_code->gran = 1;
    ring3_code->base_high = 0;

    *ring3_data = *ring3_code;
    ring3_data->code = 0;

    ring3_data->long_mode = 0;

    ring3_data->big = 1;

    init_tss();

    write_tss(
        (tss_descriptor_t *)&gdt[5]
    );
    setGdt(&gdtr);

    reloadSegments();

    flush_tss();
}