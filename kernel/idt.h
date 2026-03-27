#pragma once
#include "types.h"
#include <stdint.h>

struct idt_entry {
  uint16_t offset_low;
  uint16_t selector;
  uint8_t ist;
  uint8_t type_attr;
  uint16_t offset_mid;
  uint32_t offset_high;
  uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
  uint16_t limit;
  uint64_t base;
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
} interrupt_frame;
typedef void (*interrupt_handler_t)(interrupt_frame *frame);

uint16_t idt_init(void);
void register_interrupt_handler(int vector, interrupt_handler_t handler);
void register_default_handler(interrupt_handler_t handler);