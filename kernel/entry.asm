global _start
global enter_higher_half
global get_kernel_main_addr
global enter_kernel_main

extern bootstrap_main
extern kernel_pre_main
extern kernel_main

section .bootstrap.text
bits 64

_start:
    cli
    mov rsp, stack_top
    call bootstrap_main

.hang:
    cli
    hlt
    jmp .hang

enter_higher_half:
    mov rax, kernel_pre_main
    jmp rax

get_kernel_main_addr:
    mov rax, kernel_pre_main
    ret

section .text
bits 64

enter_kernel_main:
    mov rsp, rdi
    jmp kernel_main

section .bootstrap.bss
align 16
stack_bottom:
    resb 16384
stack_top: