#pragma once
#include "types.h"

struct idt_entry {
  ushort offset_low;
  ushort selector;
  ubyte ist;
  ubyte type_attr;
  ushort offset_mid;
  uint offset_high;
  uint zero;
} __attribute__((packed));

struct idt_ptr {
  ushort limit;
  ulong base;
} __attribute__((packed));

typedef struct interrupt_frame {
  ulong rax;
  ulong rbx;
  ulong rcx;
  ulong rdx;
  ulong rsi;
  ulong rdi;
  ulong rbp;
  ulong r8;
  ulong r9;
  ulong r10;
  ulong r11;
  ulong r12;
  ulong r13;
  ulong r14;
  ulong r15;
  ulong vector;
  ulong error_code;

  ulong rip;
  ulong cs;
  ulong rflags;
  ulong rsp; // only valid if coming from lower privilege
  ulong ss;  // only valid if coming from lower privilege
} interrupt_frame;
typedef void (*interrupt_handler_t)(interrupt_frame *frame);

ushort idt_init(void);
void register_interrupt_handler(int vector, interrupt_handler_t handler);
void register_default_handler(interrupt_handler_t handler);