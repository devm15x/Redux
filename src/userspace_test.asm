bits 64

global user_test_start
global user_test_end

section .text

user_test_start:
    lea rdi, [rel message]
    mov rax, 1
    syscall

    mov rax, 2
    syscall

message:
    db "Userspace loaded sucsessfully", 0

user_test_end: