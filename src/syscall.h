#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

void syscall_init(void);
uint64_t syscall_dispatch(uint64_t number, uint64_t arg1);

#endif