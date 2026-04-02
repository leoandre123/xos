BITS 64

global syscall_entry
extern syscall_dispatch
extern g_syscall_kernel_rsp

section .text

syscall_entry:
    ; On entry (set by the syscall instruction):
    ;   RCX = user RIP to return to
    ;   R11 = user RFLAGS
    ;   RSP = user RSP (NOT switched)
    ;   RAX = syscall number
    ;   RDI = arg1, RSI = arg2, RDX = arg3
    ;   IF is cleared

    ; Switch to kernel stack.
    ; Use R10 as scratch (caller-saved, already listed as clobber in user syscall asm).
    ; Push user RSP onto the kernel stack so it is saved per-task — a global would be
    ; overwritten if schedule() switches to another task that also makes a syscall.
    mov r10, rsp
    mov rsp, [g_syscall_kernel_rsp]
    push r10                ; user RSP now lives on this task's kernel stack

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

    ; Restore user RSP from kernel stack and switch back to user stack in one step.
    pop rsp

    o64 sysret

section .note.GNU-stack noalloc noexec nowrite progbits
