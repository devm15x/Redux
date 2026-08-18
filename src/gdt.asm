bits 64

section .text

global setGdt
global reloadSegments

setGdt:
    lgdt [rdi]
    ret

reloadSegments:
    push qword 0x08

    lea rax, [rel .reload_cs]
    push rax

    retfq

.reload_cs:
    mov ax, 0x10

    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ret