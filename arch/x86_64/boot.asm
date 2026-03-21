global _start
extern kernel_main

section .text
bits 64

_start:
    mov rsp, stack_top
    call kernel_main

.hang:
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384
stack_top: