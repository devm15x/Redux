#include "panic.h"
#include "terminal.h"

#include <stdint.h>
#include <stddef.h>

#define PANIC_BACKGROUND 0x00800000
#define PANIC_FOREGROUND 0x00FFFFFF

static const char *exception_names[32] =
{
    "Divide Error",
    "Debug Exception",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "BOUND Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point Error",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"
};

__attribute__((noreturn))
static void panic_halt(void)
{
    for (;;)
    {
        __asm__ volatile("cli; hlt");
    }
}

static void panic_clear(void)
{
    struct limine_framebuffer *framebuffer =
        terminal_get_framebuffer();

    if (framebuffer == NULL)
    {
        return;
    }

    terminal_set_colors(
        PANIC_FOREGROUND,
        PANIC_BACKGROUND
    );

    terminal_clear(
        framebuffer,
        PANIC_BACKGROUND
    );
}

static void print_hex64(uint64_t value)
{
    static const char digits[] =
        "0123456789ABCDEF";

    char buffer[19];

    buffer[0] = '0';
    buffer[1] = 'x';

    for (int i = 0; i < 16; i++)
    {
        uint64_t shift =
            (uint64_t)(15 - i) * 4;

        buffer[i + 2] =
            digits[
                (value >> shift) &
                0x0F
            ];
    }

    buffer[18] = '\0';

    print(buffer);
}

static uint64_t read_cr0(void)
{
    uint64_t value;

    __asm__ volatile(
        "mov %%cr0, %0"
        : "=r"(value)
    );

    return value;
}

static uint64_t read_cr2(void)
{
    uint64_t value;

    __asm__ volatile(
        "mov %%cr2, %0"
        : "=r"(value)
    );

    return value;
}

static uint64_t read_cr3(void)
{
    uint64_t value;

    __asm__ volatile(
        "mov %%cr3, %0"
        : "=r"(value)
    );

    return value;
}

static uint64_t read_cr4(void)
{
    uint64_t value;

    __asm__ volatile(
        "mov %%cr4, %0"
        : "=r"(value)
    );

    return value;
}

static void print_register(
    const char *name,
    uint64_t value
)
{
    print(name);
    print(": ");
    print_hex64(value);
    println("");
}

static void print_register_pair(
    const char *name1,
    uint64_t value1,
    const char *name2,
    uint64_t value2
)
{
    print(name1);
    print(": ");
    print_hex64(value1);

    print("  ");

    print(name2);
    print(": ");
    print_hex64(value2);

    println("");
}

static void print_separator(void)
{
    println(
        "----------------------------------------"
    );
}

static void print_page_fault_info(
    uint64_t error_code
)
{
    print("PF: ");

    if (error_code & (1ULL << 0))
    {
        print("protection ");
    }
    else
    {
        print("not-present ");
    }

    if (error_code & (1ULL << 1))
    {
        print("write ");
    }
    else
    {
        print("read ");
    }

    if (error_code & (1ULL << 2))
    {
        print("user ");
    }
    else
    {
        print("supervisor ");
    }

    if (error_code & (1ULL << 4))
    {
        print("instruction-fetch ");
    }

    println("");
}

__attribute__((noreturn))
void panic(
    const char *message
)
{
    __asm__ volatile("cli");

    panic_clear();

    println(
        "========================================"
    );

    println(
        "          KERNEL PANIC"
    );

    println(
        "========================================"
    );

    if (message != NULL)
    {
        println(message);
    }
    else
    {
        println("Unknown kernel panic.");
    }

    print_separator();

    println(
        "The kernel cannot continue safely."
    );

    println("HALT!!!");

    panic_halt();
}

__attribute__((noreturn))
void panic_exception(
    interrupt_frame_t *frame
)
{
    __asm__ volatile("cli");

    panic_clear();

    println(
        "========================================"
    );

    println(
        "          KERNEL PANIC"
    );

    println(
        "========================================"
    );

    if (frame == NULL)
    {
        println(
            "panic_exception received a NULL frame."
        );

        println("HALT!!!");

        panic_halt();
    }

    /*
     * Exception information
     */

    print("Exception: ");

    if (frame->vector < 32)
    {
        println(
            exception_names[
                frame->vector
            ]
        );
    }
    else
    {
        println("Unknown");
    }

    print("Vector: ");
    print_uint64(frame->vector);

    print("  Error: ");
    print_hex64(frame->error_code);

    println("");

    print_separator();

    /*
     * Main CPU state
     */

    print_register(
        "RIP",
        frame->rip
    );

    print_register_pair(
        "CS",
        frame->cs,
        "RFLAGS",
        frame->rflags
    );

    print_separator();

    /*
     * General purpose registers
     */

    print_register_pair(
        "RAX",
        frame->rax,
        "RBX",
        frame->rbx
    );

    print_register_pair(
        "RCX",
        frame->rcx,
        "RDX",
        frame->rdx
    );

    print_register_pair(
        "RSI",
        frame->rsi,
        "RDI",
        frame->rdi
    );

    print_register_pair(
        "RBP",
        frame->rbp,
        "R8",
        frame->r8
    );

    print_register_pair(
        "R9",
        frame->r9,
        "R10",
        frame->r10
    );

    print_register_pair(
        "R11",
        frame->r11,
        "R12",
        frame->r12
    );

    print_register_pair(
        "R13",
        frame->r13,
        "R14",
        frame->r14
    );

    print_register(
        "R15",
        frame->r15
    );

    print_separator();

    /*
     * Control registers
     */

    print_register_pair(
        "CR0",
        read_cr0(),
        "CR2",
        read_cr2()
    );

    print_register_pair(
        "CR3",
        read_cr3(),
        "CR4",
        read_cr4()
    );

    /*
     * Extra page-fault information only when useful.
     */

    if (frame->vector == 14)
    {
        print_page_fault_info(
            frame->error_code
        );
    }

    print_separator();

    println(
        "The system has encountured an error and has halted to prevent damage."
    );

    println("HALT!!!");

    panic_halt();
}