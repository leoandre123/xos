BITS 64

global isr0
global isr3
global isr13
global isr14
global isr33
global isr32

extern isr0_handler
extern isr3_handler
extern isr13_handler
extern isr14_handler
extern isr33_handler
extern isr32_handler

section .text

isr0:
    cli
    call isr0_handler
    iretq

isr3:
    cli
    call isr3_handler
    iretq

isr13:
    cli
    mov rdi, [rsp]        ; error code
    call isr13_handler
    add rsp, 8            ; pop error code before iretq
    iretq

isr14:
    cli
    mov rdi, [rsp]        ; error code
    call isr14_handler
    add rsp, 8            ; pop error code before iretq
    iretq

isr33:
    cli
    call isr33_handler
    iretq

isr32:
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax

    call isr32_handler

    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15

    iretq

section .note.GNU-stack noalloc noexec nowrite progbits