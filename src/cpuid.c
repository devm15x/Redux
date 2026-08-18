#include <stdint.h>

void cpuid(
    uint32_t leaf,
    uint32_t *eax,
    uint32_t *ebx,
    uint32_t *ecx,
    uint32_t *edx
)
{
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;

    __asm__ volatile (
        "cpuid"
        : "=a"(a),
          "=b"(b),
          "=c"(c),
          "=d"(d)
        : "a"(leaf),
          "c"(0)
    );

    if (eax != 0)
        *eax = a;

    if (ebx != 0)
        *ebx = b;

    if (ecx != 0)
        *ecx = c;

    if (edx != 0)
        *edx = d;
}


void cpuGetMSR(
    uint32_t msr,
    uint32_t *eax,
    uint32_t *edx
)
{
    uint32_t low;
    uint32_t high;

    __asm__ volatile (
        "rdmsr"
        : "=a"(low),
          "=d"(high)
        : "c"(msr)
    );

    if (eax != 0)
        *eax = low;

    if (edx != 0)
        *edx = high;
}


void cpuSetMSR(
    uint32_t msr,
    uint32_t eax,
    uint32_t edx
)
{
    __asm__ volatile (
        "wrmsr"
        :
        : "c"(msr),
          "a"(eax),
          "d"(edx)
        : "memory"
    );
}