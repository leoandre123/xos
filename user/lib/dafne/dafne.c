#include "dafne.h"
#include "dafne/dafne_comp_event.h"
#include "handles.h"
#include "ipc/channel.h"
#include "string.h"

static channel_handle s_channel = INVALID_HANDLE;

bool dafne_connect() {
  if (s_channel == INVALID_HANDLE)
    s_channel = ipc_connect("dafne");
  if (s_channel == INVALID_HANDLE)
    return false;
  return true;
}
void dafne_window_create(ushort width, ushort height, const char *title) {
  comp_event ev;
  ev.type = COMP_WINDOW_CREATE;
  ev.create_window.width = width;
  ev.create_window.height = height;
  strcpy(ev.create_window.title, title);
  ev.create_window.double_buffer = true;
  channel_send(s_channel, &ev, sizeof(comp_event));
}
bool dafne_wait_event(window_event *ev) {
  int read = channel_recv(s_channel, ev, sizeof(window_event));
  return read == sizeof(window_event);
}
void dafne_window_present(window_handle h) {
  comp_event ev;
  ev.type = COMP_WINDOW_PRESENT;
  ev.present_window.handle = h;
  channel_send(s_channel, &ev, sizeof(comp_event));
}