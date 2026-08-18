#ifndef REDUX_APIC_H
#define REDUX_APIC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

bool check_apic(void);

uintptr_t cpu_get_apic_base(void);

void cpu_set_apic_base(
    uintptr_t apic
);

bool enable_apic(void);

uint32_t lapic_read(
    uint32_t offset
);

void lapic_write(
    uint32_t offset,
    uint32_t value
);

void apic_set_virtual_base(
    uintptr_t virtual_base
);


void apic_eoi(void);

uint32_t cpuReadIoApic(
    void *ioapicaddr,
    uint32_t reg
);

void cpuWriteIoApic(
    void *ioapicaddr,
    uint32_t reg,
    uint32_t value
);

#endif