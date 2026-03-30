BITS 64

global syscall_entry
extern syscall_dispatch
extern g_syscall_kernel_rsp

section .data
g_syscall_user_rsp: dq 0

section .text

syscall_entry:
    ; On entry (set by the syscall instruction):
    ;   RCX = user RIP to return to
    ;   R11 = user RFLAGS
    ;   RSP = user RSP (NOT switched)
    ;   RAX = syscall number
    ;   RDI = arg1, RSI = arg2, RDX = arg3
    ;   IF is cleared

    ; Switch to kernel stack
    mov [g_syscall_user_rsp], rsp
    mov rsp, [g_syscall_kernel_rsp]
    ; Save return address, flags, and callee-saved registers
    push rcx
    push r11
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; Save user arg registers so we can restore them after sysret
    ; (Linux syscall ABI: user code expects RDI, RSI, RDX preserved)
    push rdi
    push rsi
    push rdx

    ; Shuffle args to match C calling convention:
    ;   syscall_dispatch(num, arg1, arg2, arg3)
    ;   RDI=num, RSI=arg1, RDX=arg2, RCX=arg3
    mov rcx, rdx    ; arg3 (save first, rcx is about to be overwritten)
    mov rdx, rsi    ; arg2
    mov rsi, rdi    ; arg1
    mov rdi, rax    ; syscall number

    call syscall_dispatch
    ; return value is in RAX

    pop rdx
    pop rsi
    pop rdi

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    pop r11
    pop rcx

    ; Restore user stack
    mov rsp, [g_syscall_user_rsp]
  
    o64 sysret

section .note.GNU-stack noalloc noexec nowrite progbits
