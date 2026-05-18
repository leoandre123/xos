#include "mouse.h"
#include "cpu/idt.h"
#include "io.h"
#include "io/serial.h"
#include "pic.h"
#include "types.h"

// Screen bounds for clamping — set by mouse_init from kernel globals
extern uint g_fb_width;
extern uint g_fb_height;

static volatile mouse_state g_mouse = {0};
static bool g_intellimouse = false;
static ubyte g_packet[4];
static int g_packet_idx = 0;

// --------------------------------------------------------------------------
// 8042 PS/2 controller helpers
// --------------------------------------------------------------------------

static void ps2_wait_write(void) {
  int t = 100000;
  while (t-- && (inb(0x64) & 0x02))
    ; // wait for input buffer empty
}

static void ps2_wait_read(void) {
  int t = 100000;
  while (t-- && !(inb(0x64) & 0x01))
    ; // wait for output buffer full
}

static void mouse_cmd(ubyte cmd) {
  ps2_wait_write();
  outb(0x64, 0xD4); // next byte goes to mouse
  ps2_wait_write();
  outb(0x60, cmd);
}

static ubyte mouse_ack(void) {
  ps2_wait_read();
  return inb(0x60);
}

// --------------------------------------------------------------------------
// IRQ12 handler — accumulates 3-byte packets
// --------------------------------------------------------------------------

static void on_mouse_irq(interrupt_frame *frame) {
  (void)frame;
  ubyte data = inb(0x60);

  // Sync: byte 0 must have bit 3 set (always-1 bit in PS/2 packets).
  // Discard stray bytes until we find a valid packet start.
  if (g_packet_idx == 0 && !(data & 0x08)) {
    pic_send_eoi(12);
    return;
  }

  g_packet[g_packet_idx++] = data;

  int packet_size = g_intellimouse ? 4 : 3;
  if (g_packet_idx == packet_size) {
    g_packet_idx = 0;

    ubyte flags = g_packet[0];

    // overflow bits set — discard packet
    if (flags & 0xC0) {
      pic_send_eoi(12);
      return;
    }

    // 9-bit signed dx/dy: magnitude in bytes 1/2, sign bit in flags
    int dx = (int)g_packet[1] - ((flags & 0x10) ? 256 : 0);
    int dy = (int)g_packet[2] - ((flags & 0x20) ? 256 : 0);
    dy = -dy; // PS/2 y-axis is inverted (positive = up)

    int nx = g_mouse.x + dx;
    int ny = g_mouse.y + dy;

    // clamp to framebuffer bounds
    if (nx < 0)
      nx = 0;
    if (ny < 0)
      ny = 0;
    if ((uint)nx >= g_fb_width)
      nx = (int)g_fb_width - 1;
    if ((uint)ny >= g_fb_height)
      ny = (int)g_fb_height - 1;

    g_mouse.x = nx;
    g_mouse.y = ny;
    g_mouse.buttons = flags & 0x07;

    if (g_intellimouse) {
      int dz = g_packet[3] & 0x0F;
      if (dz & 0x08)
        dz -= 16; // sign-extend 4-bit value
      g_mouse.scroll += dz;
    }

    g_mouse.pending = 1;
  }

  pic_send_eoi(12);
}

// --------------------------------------------------------------------------
// Public API
// --------------------------------------------------------------------------

void mouse_init(void) {
  // Flush any stale bytes sitting in the 8042 output buffer before we start
  // issuing commands. If we skip this, the "read command byte" response can
  // land behind a leftover keyboard scancode and we'd read the wrong byte,
  // then write it back corrupted — wiping keyboard IRQ1 enable (bit 0).
  while (inb(0x64) & 0x01)
    inb(0x60);

  // Enable auxiliary PS/2 device
  ps2_wait_write();
  outb(0x64, 0xA8);

  // Read controller command byte, enable IRQ12 and mouse clock.
  // Explicitly preserve keyboard bits so we don't accidentally disable it.
  ps2_wait_write();
  outb(0x64, 0x20);
  ps2_wait_read();
  ubyte cfg = inb(0x60);
  cfg |= 0x01;  // keep keyboard IRQ1 enabled
  cfg |= 0x02;  // enable mouse IRQ12
  cfg &= ~0x10; // keep keyboard clock enabled
  cfg &= ~0x20; // enable mouse clock
  ps2_wait_write();
  outb(0x64, 0x60);
  ps2_wait_write();
  outb(0x60, cfg);

  // Set defaults
  mouse_cmd(0xF6);
  mouse_ack();

  // IntelliMouse detection: magic sample-rate sequence 200→100→80, then read ID
  mouse_cmd(0xF3);
  mouse_ack();
  mouse_cmd(200);
  mouse_ack();
  mouse_cmd(0xF3);
  mouse_ack();
  mouse_cmd(100);
  mouse_ack();
  mouse_cmd(0xF3);
  mouse_ack();
  mouse_cmd(80);
  mouse_ack();
  mouse_cmd(0xF2);
  mouse_ack();            // request device ID (ACK)
  ubyte id = mouse_ack(); // actual ID: 0x03 = IntelliMouse
  g_intellimouse = (id == 0x03);
  serial_printf("mouse: device id=0x%02x intellimouse=%d\n", id, g_intellimouse);

  // Enable data reporting
  mouse_cmd(0xF4);
  mouse_ack();

  // Start in the center of the screen
  g_mouse.x = (int)(g_fb_width / 2);
  g_mouse.y = (int)(g_fb_height / 2);

  register_interrupt_handler(44, on_mouse_irq);
}

mouse_state mouse_read_state(void) {
  mouse_state s = g_mouse;
  g_mouse.pending = 0;
  g_mouse.scroll = 0;
  return s;
}
