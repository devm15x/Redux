#include "syscall.h"
#include "terminal.h"

#include <stdint.h>

#define IA32_EFER   0xC0000080
#define IA32_STAR   0xC0000081
#define IA32_LSTAR  0xC0000082
#define IA32_FMASK  0xC0000084

#define EFER_SCE    (1ULL << 0)

#define SYSCALL_STACK_SIZE 16384

static uint8_t syscall_stack[SYSCALL_STACK_SIZE]
    __attribute__((aligned(16)));


uint64_t syscall_kernel_stack_top = 0;

extern void syscall_entry(void);

static uint64_t rdmsr(uint32_t msr)
{
    uint32_t low;
    uint32_t high;

    __asm__ volatile(
        "rdmsr"
        : "=a"(low),
          "=d"(high)
        : "c"(msr)
    );

    return ((uint64_t)high << 32) | low;
}

static void wrmsr(
    uint32_t msr,
    uint64_t value
)
{
    uint32_t low =
        (uint32_t)value;

    uint32_t high =
        (uint32_t)(value >> 32);

    __asm__ volatile(
        "wrmsr"
        :
        : "c"(msr),
          "a"(low),
          "d"(high)
    );
}

void syscall_init(void)
{
    syscall_kernel_stack_top =
        (uint64_t)(
            syscall_stack +
            SYSCALL_STACK_SIZE
        );


    uint64_t efer =
        rdmsr(IA32_EFER);

    efer |= EFER_SCE;

    wrmsr(
        IA32_EFER,
        efer
    );


    wrmsr(
        IA32_STAR,
        ((uint64_t)0x08 << 32)
    );


    wrmsr(
        IA32_LSTAR,
        (uint64_t)(uintptr_t)syscall_entry
    );

    wrmsr(
        IA32_FMASK,
        0x200
    );
}

uint64_t syscall_dispatch(uint64_t number, uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
    if (number == 1)
    {
        println((const char *)arg1);
        return 0;
    }

    if (number == 2)
    {
        return 1;
    }
    if (number == 3)
    {
        print((const char *)arg1);
        return 0;
    }
    if (number == 4)
    {
        terminal_putchar(arg1);
        return 0;
    }
    if (number == 5) {
        terminal_clear(terminal_get_framebuffer(), 0x00081A33);
        return 0;
    }
    if (number == 6) {
        terminal_putpixel(arg1, arg2, arg3);
        return 0;
    }
}