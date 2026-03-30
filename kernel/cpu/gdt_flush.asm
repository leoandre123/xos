BITS 64

global jump_to_userspace
global tss_load
global gdt_load_flush

section .text

jump_to_userspace:
    mov ax, 0x23
    mov ds, ax
    mov es, ax

    push 0x23       ; ss  (user data | RPL=3)
    push rsi        ; rsp (user stack)
    push 0x202      ; rflags (IF=1)
    push 0x2B       ; cs  (user code 64 | RPL=3)
    push rdi        ; rip (entry point)
    iretq

tss_load:
    ltr di
    ret

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
    
section .note.GNU-stack noalloc noexec nowrite progbits