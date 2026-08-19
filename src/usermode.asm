bits 64

global jump_usermode

jump_usermode:

    push qword 0x23
    push rsi
    push qword 0x2
    push qword 0x1B
    push rdi

    iretq