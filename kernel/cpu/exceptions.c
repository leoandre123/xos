#include "exceptions.h"
#include "idt.h"
#include "io/serial.h"
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

static void dump_frame(interrupt_frame *f) {
  serial_write_line("=== INTERRUPT FRAME ===");

  serial_write("vector: ");
  serial_write_ulong(f->vector);
  serial_write("\n");

  serial_write("error: ");
  serial_write_hex(f->error_code);
  serial_write("\n");

  // CPU-pushed iretq frame sits just after our saved registers
  ulong *cpu_frame = (ulong *)((ulong)f + sizeof(interrupt_frame));
  serial_write("rip:    ");
  serial_write_hex(cpu_frame[0]);
  serial_write("\n");
  serial_write("cs:     ");
  serial_write_hex(cpu_frame[1]);
  serial_write("\n");
  serial_write("rflags: ");
  serial_write_hex(cpu_frame[2]);
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

void breakpoint_handler(interrupt_frame *frame) {
  serial_write_line(g_exception_names[frame->vector]);
  dump_frame(frame);
}

void exceptions_init() {
  register_default_handler((interrupt_handler_t)default_handler);
  register_interrupt_handler(3, (interrupt_handler_t)breakpoint_handler);
}