#include "panic.h"
#include "debug/disasm.h"
#include "graphics/console.h"
#include "io/serial.h"

static void preg(const char *name, ulong val) {
  console_write(name);
  console_write("=");
  console_write_hex64(val);
  console_write("  ");
}


void panic_impl(const char *msg, const panic_regs *r) {
  __asm__ volatile("cli");

  console_clear(0x00C0392B);
  console_set_bg_color(0x00C0392B);
  console_set_fg_color(0x00FFFFFF);
  console_write_line("*** KERNEL PANIC ***");
  console_write_line(msg);
  console_write("\n");

  // General purpose registers
  preg("rax", r->rax); preg("rbx", r->rbx);
  preg("rcx", r->rcx); preg("rdx", r->rdx);
  console_write("\n");
  preg("rsi", r->rsi); preg("rdi", r->rdi);
  preg("rbp", r->rbp); preg("rsp", r->rsp);
  console_write("\n");
  preg("r8 ", r->r8);  preg("r9 ", r->r9);
  preg("r10", r->r10); preg("r11", r->r11);
  console_write("\n");
  preg("r12", r->r12); preg("r13", r->r13);
  preg("r14", r->r14); preg("r15", r->r15);
  console_write("\n");
  preg("rip", r->rip);
  preg("rfl", r->rflags);
  console_write("\n\n");

  // Disassembly around RIP
  console_set_fg_color(0x00FFDD57);
  disasm_around(r->rip, 5);

  // Mirror to serial so it's in the QEMU log
  serial_printf("PANIC: %s\n", msg);
  serial_printf("  rax=%016x  rbx=%016x  rcx=%016x  rdx=%016x\n",
                r->rax, r->rbx, r->rcx, r->rdx);
  serial_printf("  rsi=%016x  rdi=%016x  rbp=%016x  rsp=%016x\n",
                r->rsi, r->rdi, r->rbp, r->rsp);
  serial_printf("  r8 =%016x  r9 =%016x  r10=%016x  r11=%016x\n",
                r->r8,  r->r9,  r->r10, r->r11);
  serial_printf("  r12=%016x  r13=%016x  r14=%016x  r15=%016x\n",
                r->r12, r->r13, r->r14, r->r15);
  serial_printf("  rip=%016x  rflags=%016x\n", r->rip, r->rflags);

  for (;;)
    __asm__ volatile("hlt");
}

void panic_assert(const char *file, int line, const char *expr) {
  console_set_bg_color(0x00C0392B);
  console_set_fg_color(0x00FFFFFF);
  console_write("ASSERT FAILED: ");
  console_write_line(expr);
  console_write("FILE: ");
  console_write_line(file);
  console_write("LINE: ");
  console_write_u32(line);
  console_write("\n");

  __asm__ volatile("cli");
  for (;;)
    __asm__ volatile("hlt");
}
