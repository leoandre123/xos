#pragma once

#include "scheduler/task.h"
typedef int KeyCode;

typedef struct {
  KeyCode code;
  char character;
} KeyEvent;

void keyboard_init(void);
KeyEvent keyboard_read(void);
void keyboard_inject(KeyEvent ev);

bool keyboard_set_reader(task *t);