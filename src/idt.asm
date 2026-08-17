bits 64

section .text

extern exception_handler


; ------------------------------------------------------------
; Exception WITHOUT a CPU-pushed error code
;
; We push a fake error code of 0 so every exception has the
; same stack layout.
; ------------------------------------------------------------

%macro isr_no_err_stub 1
global isr_stub_%1

isr_stub_%1:
    push qword 0
    push qword %1
    jmp isr_common
%endmacro


; ------------------------------------------------------------
; Exception WITH a CPU-pushed error code
;
; CPU already pushed:
;     error_code
;
; So we only add:
;     vector
; ------------------------------------------------------------

%macro isr_err_stub 1
global isr_stub_%1

isr_stub_%1:
    push qword %1
    jmp isr_common
%endmacro


; ------------------------------------------------------------
; Common exception handler
; ------------------------------------------------------------

isr_common:

    ; Save general-purpose registers.
    ;
    ; This exact order is important because panic.h must use
    ; the corresponding interrupt_frame_t layout.

    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rsi
    push rdi

    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15


    ; System V AMD64 ABI:
    ;
    ; RDI = first function argument
    ;
    ; RSP currently points at our complete saved frame.

    mov rdi, rsp

    call exception_handler


    ; Normally exception_handler() panics and never returns.
    ;
    ; But keeping the restore path here makes the stub correct
    ; if you later allow recoverable exceptions.

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8

    pop rdi
    pop rsi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax


    ; Remove:
    ;
    ; vector
    ; error_code
    ;
    ; For exceptions with a CPU error code, this removes both
    ; the vector we added and the CPU error code.
    ;
    ; For exceptions without one, it removes the vector and
    ; our fake zero error code.

    add rsp, 16

    iretq


; ============================================================
; CPU exceptions 0 - 31
; ============================================================

isr_no_err_stub 0       ; #DE Divide Error
isr_no_err_stub 1       ; #DB Debug
isr_no_err_stub 2       ; NMI
isr_no_err_stub 3       ; #BP Breakpoint
isr_no_err_stub 4       ; #OF Overflow
isr_no_err_stub 5       ; #BR Bounds
isr_no_err_stub 6       ; #UD Invalid Opcode
isr_no_err_stub 7       ; #NM Device Not Available

isr_err_stub    8       ; #DF Double Fault

isr_no_err_stub 9       ; Reserved / old coprocessor overrun

isr_err_stub    10      ; #TS Invalid TSS
isr_err_stub    11      ; #NP Segment Not Present
isr_err_stub    12      ; #SS Stack Segment Fault
isr_err_stub    13      ; #GP General Protection
isr_err_stub    14      ; #PF Page Fault

isr_no_err_stub 15      ; Reserved
isr_no_err_stub 16      ; #MF x87 Floating Point

isr_err_stub    17      ; #AC Alignment Check

isr_no_err_stub 18      ; #MC Machine Check
isr_no_err_stub 19      ; #XM/#XF SIMD Floating Point
isr_no_err_stub 20      ; #VE Virtualization Exception

isr_err_stub    21      ; #CP Control Protection

isr_no_err_stub 22
isr_no_err_stub 23
isr_no_err_stub 24
isr_no_err_stub 25
isr_no_err_stub 26
isr_no_err_stub 27
isr_no_err_stub 28

; AMD-specific exceptions may use these vectors.
; Keep them as error-code exceptions if you want to support them.
isr_err_stub    29      ; #VC VMM Communication
isr_err_stub    30      ; #SX Security Exception

isr_no_err_stub 31


; ============================================================
; ISR address table used by idt.c
; ============================================================

section .rodata

global isr_stub_table

isr_stub_table:

%assign i 0
%rep 32
    dq isr_stub_%+i
%assign i i + 1
%endrep