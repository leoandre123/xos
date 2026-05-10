#include "exceptions.h"
#include "graphics/console.h"
#include "idt.h"
#include "io/serial.h"
#include "panic.h"

static const char *g_exception_names[32] = {
    "Divide Error",                   // 0
    "Debug",                          // 1
    "NMI",                            // 2
    "Breakpoint",                     // 3
    "Overflow",                       // 4
    "BOUND Range Exceeded",           // 5
    "Invalid Opcode",                 // 6
    "Device Not Available",           // 7
    "Double Fault",                   // 8
    "Coprocessor Segment Overrun",    // 9
    "Invalid TSS",                    // 10
    "Segment Not Present",            // 11
    "Stack-Segment Fault",            // 12
    "General Protection Fault",       // 13
    "Page Fault",                     // 14
    "Reserved",                       // 15
    "x87 Floating-Point Exception",   // 16
    "Alignment Check",                // 17
    "Machine Check",                  // 18
    "SIMD Floating-Point Exception",  // 19
    "Virtualization Exception",       // 20
    "Control Protection Exception",   // 21
    "Reserved",                       // 22
    "Reserved",                       // 23
    "Reserved",                       // 24
    "Reserved",                       // 25
    "Reserved",                       // 26
    "Reserved",                       // 27
    "Hypervisor Injection Exception", // 28
    "VMM Communication Exception",    // 29
    "Security Exception",             // 30
    "Reserved"                        // 31
};

#define KERN_BASE 0xFFFF800000000000UL

static void stack_trace(interrupt_frame *f) {
  serial_write_line("=== STACK TRACE ===");

  // For a null function pointer call the `call` instruction pushes the return
  // address before the CPU takes the fault, so the stack slot that would hold
  // RSP in a privilege-change frame instead holds that return address.
  if (f->rip == 0) {
    serial_write("  [call site] ");
    serial_write_hex(f->rsp);
    serial_write_char('\n');
  }

  // Walk the RBP chain.
  ulong *fp = (ulong *)f->rbp;
  for (int depth = 0; depth < 24; depth++) {
    if (!fp || (ulong)fp < KERN_BASE || (ulong)fp & 7)
      break;
    serial_write("  [");
    serial_write_ulong(depth);
    serial_write("] ");
    serial_write_hex(fp[1]);  // [rbp+8] = return address
    serial_write_char('\n');
    fp = (ulong *)fp[0];      // [rbp+0] = previous rbp
  }
}

static void dump_frame(interrupt_frame *f) {
  serial_write_line("=== INTERRUPT FRAME ===");

  serial_write("vector: ");
  serial_write_ulong(f->vector);
  serial_write("\n");

  serial_write("error: ");
  serial_write_hex(f->error_code);
  serial_write("\n");

  serial_write("rip:    ");
  serial_write_hex(f->rip);
  serial_write("\n");
  serial_write("cs:     ");
  serial_write_hex(f->cs);
  serial_write("\n");
  serial_write("rflags: ");
  serial_write_hex(f->rflags);
  serial_write("\n");
  serial_write("rsp: ");
  serial_write_hex(f->rsp);
  serial_write("\n");
  serial_write("ss: ");
  serial_write_hex(f->ss);
  serial_write("\n");

  ulong cr2;
  asm volatile("mov %%cr2, %0" : "=r"(cr2));
  serial_write("cr2:    ");
  serial_write_hex(cr2);
  serial_write("\n");

  serial_write("rax: ");
  serial_write_hex(f->rax);
  serial_write("\n");
  serial_write("rbx: ");
  serial_write_hex(f->rbx);
  serial_write("\n");
  serial_write("rcx: ");
  serial_write_hex(f->rcx);
  serial_write("\n");
  serial_write("rdx: ");
  serial_write_hex(f->rdx);
  serial_write("\n");
  serial_write("rsi: ");
  serial_write_hex(f->rsi);
  serial_write("\n");
  serial_write("rdi: ");
  serial_write_hex(f->rdi);
  serial_write("\n");
  serial_write("rbp: ");
  serial_write_hex(f->rbp);
  serial_write("\n");
  serial_write("r8: ");
  serial_write_hex(f->r8);
  serial_write("\n");
  serial_write("r9: ");
  serial_write_hex(f->r9);
  serial_write("\n");
  serial_write("r10: ");
  serial_write_hex(f->r10);
  serial_write("\n");
  serial_write("r11: ");
  serial_write_hex(f->r11);
  serial_write("\n");
  serial_write("r12: ");
  serial_write_hex(f->r12);
  serial_write("\n");
  serial_write("r13: ");
  serial_write_hex(f->r13);
  serial_write("\n");
  serial_write("r14: ");
  serial_write_hex(f->r14);
  serial_write("\n");
  serial_write("r15: ");
  serial_write_hex(f->r15);
  serial_write("\n");

  stack_trace(f);
}

void default_handler(interrupt_frame *frame) {
  if (frame->vector < 32) {
    serial_write(g_exception_names[frame->vector]);
    dump_frame(frame);
    for (;;) {
      asm volatile("cli; hlt");
    }
  } else {
    serial_write("IRQ - Vector: ");
    serial_write_ulong(frame->vector);
    serial_write_char('\n');
  }
}

void pagefault_handler(interrupt_frame *frame) {
  serial_write_line("===== PAGE FAULT =====");
  serial_write("Adress (CR2): ");
  ulong cr2;
  asm volatile("mov %%cr2, %0" : "=r"(cr2));
  serial_write_hex(cr2);
  serial_write_char('\n');
  serial_write("Page present: ");
  serial_write_line(frame->error_code & 1 ? "Yes" : "No");
  serial_write("Write: ");
  serial_write_line((frame->error_code >> 1) & 1 ? "Yes" : "No");
  serial_write("User: ");
  serial_write_line((frame->error_code >> 2) & 1 ? "Yes" : "No");
  serial_write("Instruction fetch: ");
  serial_write_line((frame->error_code >> 4) & 1 ? "Yes" : "No");

  serial_write("RSP: ");
  serial_write_hex(frame->rsp);
  serial_write_char('\n');

  dump_frame(frame);

  panic("PAGE FAULT");

  for (;;) {
    asm volatile("cli; hlt");
  }
}
void pagefault_handler2(interrupt_frame *frame) {
  console_writef("===== PAGE FAULT =====\n");

  ulong cr2;
  asm volatile("mov %%cr2, %0" : "=r"(cr2));
  console_writef("CR2: %x\n", cr2);
  console_writef("RIP: %x\n", frame->rip);

  console_writef("Page present: %s\n", frame->error_code & 1 ? "Yes" : "No");
  console_writef("Write: %s\n", (frame->error_code >> 1) & 1 ? "Yes" : "No");
  console_writef("User: %s\n", (frame->error_code >> 2) & 1 ? "Yes" : "No");
  console_writef("Instruction fetch: %s\n", (frame->error_code >> 4) & 1 ? "Yes" : "No");

  console_writef("RSP: %x\n", frame->rsp);
  console_writef("RAX: %x\n", frame->rax);
  console_writef("RBX: %x\n", frame->rbx);
  console_writef("RCX: %x\n", frame->rcx);
  console_writef("RDX: %x\n", frame->rdx);

  dump_frame(frame);

  for (;;) {
    asm volatile("cli; hlt");
  }
}

void breakpoint_handler(interrupt_frame *frame) {
  serial_write_line(g_exception_names[frame->vector]);
  dump_frame(frame);
}

void exceptions_init() {
  register_default_handler((interrupt_handler_t)default_handler);
  register_interrupt_handler(EX_BREAKPOINT, (interrupt_handler_t)breakpoint_handler);
  register_interrupt_handler(EX_PAGE_FAULT, pagefault_handler2);
}