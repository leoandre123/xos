BITS 64

global isr0
global isr3
global isr13
global isr14

extern isr0_handler
extern isr3_handler
extern isr13_handler
extern isr14_handler

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