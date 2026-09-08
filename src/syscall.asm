bits 64

global syscall_entry

extern syscall_kernel_stack_top
extern syscall_dispatch
extern usermode_return_rsp
extern usermode_return_rip

section .bss
align 8

syscall_user_rsp:
    resq 1

section .text

syscall_entry:
    mov [rel syscall_user_rsp], rsp
    mov rsp, [rel syscall_kernel_stack_top]

    push rcx
    push r11
    push rdi
    push rsi
    push rdx
    push r10
    push r8
    push r9

    mov rcx, rdx
    mov rdx, rsi
    mov rsi, rdi
    mov rdi, rax
    call syscall_dispatch

    cmp rax, 1
    je .exit

    pop r9
    pop r8
    pop r10
    pop rdx
    pop rsi
    pop rdi

    pop r11
    pop rcx

    mov rsp, [rel syscall_user_rsp]

    o64 sysret

.exit:
    mov rsp, [rel usermode_return_rsp]
    sti
    jmp [rel usermode_return_rip]