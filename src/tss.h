#ifndef TSS_H
#define TSS_H

#include <stdint.h>

typedef struct
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t limit_high_flags;
    uint8_t base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed)) tss_descriptor_t;

void init_tss(void);
void write_tss(tss_descriptor_t *g);
void flush_tss(void);

#endif