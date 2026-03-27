#include "keyboard.h"
#include "idt.h"
#include "io.h"
#include "keys.h"
#include "pic.h"
#include "serial.h"

static int extended = 0;
static volatile KeyEvent g_last = {KEY_NONE, 0};

static char scancode_set1_to_ascii(uint8_t sc) {
  switch (sc) {
  case 0x02:
    return '1';
  case 0x03:
    return '2';
  case 0x04:
    return '3';
  case 0x05:
    return '4';
  case 0x06:
    return '5';
  case 0x07:
    return '6';
  case 0x08:
    return '7';
  case 0x09:
    return '8';
  case 0x0A:
    return '9';
  case 0x0B:
    return '0';

  case 0x10:
    return 'q';
  case 0x11:
    return 'w';
  case 0x12:
    return 'e';
  case 0x13:
    return 'r';
  case 0x14:
    return 't';
  case 0x15:
    return 'y';
  case 0x16:
    return 'u';
  case 0x17:
    return 'i';
  case 0x18:
    return 'o';
  case 0x19:
    return 'p';

  case 0x1E:
    return 'a';
  case 0x1F:
    return 's';
  case 0x20:
    return 'd';
  case 0x21:
    return 'f';
  case 0x22:
    return 'g';
  case 0x23:
    return 'h';
  case 0x24:
    return 'j';
  case 0x25:
    return 'k';
  case 0x26:
    return 'l';

  case 0x2C:
    return 'z';
  case 0x2D:
    return 'x';
  case 0x2E:
    return 'c';
  case 0x2F:
    return 'v';
  case 0x30:
    return 'b';
  case 0x31:
    return 'n';
  case 0x32:
    return 'm';

  case 0x39:
    return ' ';
  case 0x1C:
    return '\n';
  default:
    return 0;
  }
}

void on_key_event(void) {
  uint8_t sc = inb(0x60);

  // Extended prefix
  if (sc == 0xE0) {
    extended = 1;
    pic_send_eoi(1);
    return;
  }

  // Ignore key releases
  if (sc & 0x80) {
    extended = 0;
    pic_send_eoi(1);
    return;
  }

  KeyEvent ev = {KEY_NONE, 0};

  if (extended) {
    switch (sc) {
    case 0x48:
      ev.code = KEY_UP;
      break;
    case 0x50:
      ev.code = KEY_DOWN;
      break;
    case 0x4B:
      ev.code = KEY_LEFT;
      break;
    case 0x4D:
      ev.code = KEY_RIGHT;
      break;
    }
    extended = 0;
  } else {
    if (sc == 0x1C) {
      ev.code = KEY_RETURN;
    } else {
      char c = scancode_set1_to_ascii(sc);
      if (c) {
        ev.code = 1;
        ev.character = c;
      }
    }
  }

  if (ev.code != KEY_NONE) {
    g_last = ev;
  }

  if (ev.character)
    serial_write_char(ev.character);
  pic_send_eoi(1);
}

void keyboard_init(void) {
  register_interrupt_handler(33, (interrupt_handler_t)on_key_event);
}

KeyEvent keyboard_last(void) {
  KeyEvent event = g_last;
  g_last.code = KEY_NONE;
  g_last.character = 0;
  return event;
}
