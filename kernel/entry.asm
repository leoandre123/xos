global _start
global enter_higher_half
global get_kernel_main_addr
global get_kernel_main_byte0
global get_kernel_main_byte1
global get_kernel_main_byte2
global get_kernel_main_byte3

extern bootstrap_main
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
    mov rax, kernel_main
    jmp rax

get_kernel_main_addr:
    mov rax, kernel_main
    ret

get_kernel_main_byte0:
    mov rax, kernel_main
    movzx eax, byte [rax]
    ret

get_kernel_main_byte1:
    mov rax, kernel_main
    movzx eax, byte [rax + 1]
    ret

get_kernel_main_byte2:
    mov rax, kernel_main
    movzx eax, byte [rax + 2]
    ret

get_kernel_main_byte3:
    mov rax, kernel_main
    movzx eax, byte [rax + 3]
    ret

section .text
bits 64
kernel_main_a:
    mov dx, 0x3F8
    mov al, 'K'
    out dx, al
    mov dx, 0x3F8
    mov al, 'E'
    out dx, al
    mov rax, kernel_main
    jmp rax
    
    mov dx, 0x3F8
    mov al, 'N'
    out dx, al

.hang2:
    cli
    hlt
    jmp .hang2

section .bootstrap.bss
align 16
stack_bottom:
    resb 16384
stack_top: