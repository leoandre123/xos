#include "idt.h"
#include "io/pic.h"
#include "io/serial.h"

extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);
extern void isr32(void);
extern void isr33(void);
extern void isr34(void);
extern void isr35(void);
extern void isr36(void);
extern void isr37(void);
extern void isr38(void);
extern void isr39(void);
extern void isr40(void);
extern void isr41(void);
extern void isr42(void);
extern void isr43(void);
extern void isr44(void);
extern void isr45(void);
extern void isr46(void);
extern void isr47(void);

static interrupt_handler_t g_handlers[256];
static interrupt_handler_t g_default_handler;

static struct idt_entry idt[256];
static struct idt_ptr idtr;

static ushort read_cs(void) {
  ushort cs;
  asm volatile("mov %%cs, %0" : "=r"(cs));
  return cs;
}

static void idt_set_gate(int vector, void *isr, ubyte flags) {
  ulong addr = (ulong)isr;

  idt[vector].offset_low = addr & 0xFFFF;
  idt[vector].selector = 0x08; // kernel code segment
  idt[vector].selector = read_cs();
  idt[vector].ist = 0x00;
  idt[vector].type_attr = flags;
  idt[vector].offset_mid = (addr >> 16) & 0xFFFF;
  idt[vector].offset_high = (addr >> 32) & 0xFFFFFFFF;
  idt[vector].zero = 0;
}

void register_default_handler(interrupt_handler_t handler) {
  g_default_handler = handler;
}
void register_interrupt_handler(int vector, interrupt_handler_t handler) {
  g_handlers[vector] = handler;
}

void interrupt_dispatch(interrupt_frame *frame) {
  if (frame->vector >= 32 && frame->vector < 48) {
    pic_send_eoi(frame->vector - 32);
  }

  if (g_handlers[frame->vector]) {
    g_handlers[frame->vector](frame);
  } else if (g_default_handler) {
    g_default_handler(frame);
  } else {
    serial_write_line("NO HANDLER AVAILABLE");
  }
}

static void idt_load(struct idt_ptr *ptr) {
  asm volatile("lidt (%0)" : : "r"(ptr));
}

ushort idt_init(void) {
  ushort loc = read_cs();
  for (int i = 0; i < 256; i++) {
    idt_set_gate(i, 0, 0);
  }
  idt_set_gate(0, isr0, 0x8E);
  idt_set_gate(1, isr1, 0x8E);
  idt_set_gate(2, isr2, 0x8E);
  idt_set_gate(3, isr3, 0x8E);
  idt_set_gate(4, isr4, 0x8E);
  idt_set_gate(5, isr5, 0x8E);
  idt_set_gate(6, isr6, 0x8E);
  idt_set_gate(7, isr7, 0x8E);
  idt_set_gate(8, isr8, 0x8E);
  idt_set_gate(9, isr9, 0x8E);
  idt_set_gate(10, isr10, 0x8E);
  idt_set_gate(11, isr11, 0x8E);
  idt_set_gate(12, isr12, 0x8E);
  idt_set_gate(13, isr13, 0x8E);
  idt_set_gate(14, isr14, 0x8E);
  idt_set_gate(15, isr15, 0x8E);
  idt_set_gate(16, isr16, 0x8E);
  idt_set_gate(17, isr17, 0x8E);
  idt_set_gate(18, isr18, 0x8E);
  idt_set_gate(19, isr19, 0x8E);
  idt_set_gate(20, isr20, 0x8E);
  idt_set_gate(21, isr21, 0x8E);
  idt_set_gate(22, isr22, 0x8E);
  idt_set_gate(23, isr23, 0x8E);
  idt_set_gate(24, isr24, 0x8E);
  idt_set_gate(25, isr25, 0x8E);
  idt_set_gate(26, isr26, 0x8E);
  idt_set_gate(27, isr27, 0x8E);
  idt_set_gate(28, isr28, 0x8E);
  idt_set_gate(29, isr29, 0x8E);
  idt_set_gate(30, isr30, 0x8E);
  idt_set_gate(31, isr31, 0x8E);
  idt_set_gate(32, isr32, 0x8E);
  idt_set_gate(33, isr33, 0x8E);
  idt_set_gate(34, isr34, 0x8E);
  idt_set_gate(35, isr35, 0x8E);
  idt_set_gate(36, isr36, 0x8E);
  idt_set_gate(37, isr37, 0x8E);
  idt_set_gate(38, isr38, 0x8E);
  idt_set_gate(39, isr39, 0x8E);
  idt_set_gate(40, isr40, 0x8E);
  idt_set_gate(41, isr41, 0x8E);
  idt_set_gate(42, isr42, 0x8E);
  idt_set_gate(43, isr43, 0x8E);
  idt_set_gate(44, isr44, 0x8E);
  idt_set_gate(45, isr45, 0x8E);
  idt_set_gate(46, isr46, 0x8E);
  idt_set_gate(47, isr47, 0x8E);

  idtr.limit = sizeof(idt) - 1;
  idtr.base = (ulong)&idt[0];

  idt_load(&idtr);

  return loc;
}