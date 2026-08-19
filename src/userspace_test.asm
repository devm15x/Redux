bits 64

global user_test_start
global user_test_end

section .text

user_test_start:

    mov rax, 1
    syscall

.loop:
    pause
    jmp .loop

user_test_end: