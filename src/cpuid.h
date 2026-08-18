#ifndef REDUX_CPUID_H
#define REDUX_CPUID_H

#include <stdint.h>
#include <stdbool.h>

#define CPUID_FEAT_EDX_APIC (1u << 9)

void cpuid(
    uint32_t leaf,
    uint32_t *eax,
    uint32_t *ebx,
    uint32_t *ecx,
    uint32_t *edx
);

void cpuGetMSR(
    uint32_t msr,
    uint32_t *eax,
    uint32_t *edx
);

void cpuSetMSR(
    uint32_t msr,
    uint32_t eax,
    uint32_t edx
);

#endif
