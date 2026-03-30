#pragma once

typedef int KeyCode;

typedef struct {
  KeyCode code;
  char character;
} KeyEvent;

void keyboard_init(void);
KeyEvent keyboard_last(void);