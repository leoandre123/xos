BITS 64

global gdt_load_flush

section .text

gdt_load_flush:
    ; rdi = &gdtr
    ; rsi = data selector

    lgdt [rdi]

    mov ax, si
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Reload CS with a far return
    push 0x08
    lea rax, [rel .reload_cs]
    push rax
    retfq

.reload_cs:
    ret