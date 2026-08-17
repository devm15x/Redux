#ifndef REDUX_PANIC_H
#define REDUX_PANIC_H

#include <stdint.h>

typedef struct
{
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;

    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbp;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;

    uint64_t vector;
    uint64_t error_code;

    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;

    /*
     * These are only pushed automatically by the CPU
     * when switching privilege levels, such as Ring 3 -> Ring 0.
     *
     * Don't blindly read them for a Ring 0 -> Ring 0 exception.
     */
} interrupt_frame_t;

__attribute__((noreturn))
void panic(const char *message);

__attribute__((noreturn))
void panic_exception(
    interrupt_frame_t *frame
);

#endif