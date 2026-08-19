bits 64

global syscall_entry

extern syscall_kernel_stack_top
extern syscall_dispatch

section .text

syscall_entry:
    ; On SYSCALL:
    ;
    ; RCX = user RIP
    ; R11 = user RFLAGS
    ; RSP = STILL user RSP
    ; RAX = syscall number

    ;
    ; Save userspace RSP before switching stacks.
    ;
    mov rdx, rsp

    ;
    ; Switch to our kernel syscall stack.
    ;
    mov rsp, [rel syscall_kernel_stack_top]

    ;
    ; Construct an IRETQ frame so we can return
    ; to Ring 3 later.
    ;

    ; SS
    push qword 0x23

    ; User RSP
    push rdx

    ; User RFLAGS
    push r11

    ; User CS
    push qword 0x1B

    ; User RIP
    push rcx

    ;
    ; RAX contains syscall number.
    ; First C argument goes in RDI.
    ;
    mov rdi, rax

    ;
    ; Keep stack correctly aligned for the C call.
    ;
    sub rsp, 8

    call syscall_dispatch

    add rsp, 8

    ;
    ; Return to Ring 3.
    ;
    iretq