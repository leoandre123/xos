global _start
global enter_higher_half
global get_kernel_main_addr
global enter_kernel_main

extern bootstrap_main
extern kernel_pre_main
extern kernel_main
extern __bss_start
extern __bss_end

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
    ; Zero BSS — UEFI on real hardware does not guarantee zeroed pages.
    ; Save boot_info (RDI) across the rep stosb which clobbers RDI, RCX, RAX.
    push rdi
    mov rdi, __bss_start
    mov rcx, __bss_end
    sub rcx, rdi
    xor eax, eax
    rep stosb
    pop rdi

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