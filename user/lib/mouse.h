#pragma once

#define MOUSE_BTN_LEFT 1
#define MOUSE_BTN_RIGHT 2
#define MOUSE_BTN_MIDDLE 4

typedef struct {
  int x, y, buttons;
} mouse_event;