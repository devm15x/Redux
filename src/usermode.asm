bits 64

global jump_usermode
global usermode_return_rsp
global usermode_return_rip

section .bss
align 8

usermode_return_rsp:
    resq 1

usermode_return_rip:
    resq 1

section .text

jump_usermode:
    mov [rel usermode_return_rsp], rsp

    lea rax, [rel .returned]
    mov [rel usermode_return_rip], rax

    push qword 0x23
    push rsi
    push qword 0x202
    push qword 0x1B
    push rdi

    iretq

.returned:
    ret