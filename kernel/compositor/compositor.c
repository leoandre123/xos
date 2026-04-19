#include "compositor.h"
#include "memory/memutils.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "scheduler/task.h"
#include "types.h"
#include "window.h"

static task *g_compositor = 0;

static kernel_window g_windows[WINDOW_MAX_COUNT];

static compositor_event g_queue[COMPOSITOR_QUEUE_SIZE];
static int g_queue_head = 0;
static int g_queue_tail = 0;

static int queue_push(compositor_event *ev) {
  int next = (g_queue_head + 1) % COMPOSITOR_QUEUE_SIZE;
  if (next == g_queue_tail)
    return -1;
  g_queue[g_queue_head] = *ev;
  g_queue_head = next;
  return 0;
}

static int queue_pop(compositor_event *ev) {
  if (g_queue_head == g_queue_tail)
    return 0;
  *ev = g_queue[g_queue_tail];
  g_queue_tail = (g_queue_tail + 1) % COMPOSITOR_QUEUE_SIZE;
  return 1;
}

int compositor_register(task *t) {
  if (g_compositor)
    return -1;
  g_compositor = t;
  return 0;
}

int compositor_poll(compositor_event *ev) {
  return queue_pop(ev);
}

ulong compositor_window_create(task *client, ushort width, ushort height,
                               const char *title, window_handle *handle_out) {
  if (!g_compositor)
    return 0;

  int idx = -1;
  for (int i = 0; i < WINDOW_MAX_COUNT; i++) {
    if (!g_windows[i].exists) {
      idx = i;
      break;
    }
  }
  if (idx < 0)
    return 0;

  ulong size = (ulong)width * height * 4;
  ulong page_count = (size + PAGE_SIZE - 1) / PAGE_SIZE;

  ulong phys = pmm_alloc_pages(page_count);
  if (!phys)
    return 0;

  ulong client_vaddr = client->heap_next;
  vmm_map_pages(client->address_space, client_vaddr, phys, page_count,
                PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
  client->heap_next += page_count * PAGE_SIZE;

  ulong comp_vaddr = g_compositor->heap_next;
  vmm_map_pages(g_compositor->address_space, comp_vaddr, phys, page_count,
                PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
  g_compositor->heap_next += page_count * PAGE_SIZE;

  memset8((ubyte *)PHYS_TO_HHDM(phys), 0, page_count * PAGE_SIZE);

  kernel_window *w = &g_windows[idx];
  w->exists = true;
  w->owner_pid = client->pid;
  w->phys_base = phys;
  w->page_count = page_count;
  w->client_vaddr = client_vaddr;
  w->comp_vaddr = comp_vaddr;
  w->width = width;
  w->height = height;
  w->dirty = false;
  w->ev_head = 0;
  w->ev_tail = 0;

  *handle_out = (window_handle)idx;

  compositor_event ev = {.type = CET_WINDOW_CREATE};
  ev.create_window.handle = (window_handle)idx;
  ev.create_window.comp_vaddr = comp_vaddr;
  ev.create_window.width = width;
  ev.create_window.height = height;
  int i = 0;
  while (title[i] && i < 63) {
    ev.create_window.title[i] = title[i];
    i++;
  }
  ev.create_window.title[i] = '\0';

  queue_push(&ev);
  return client_vaddr;
}

void compositor_present_window(window_handle handle) {
  if (handle >= WINDOW_MAX_COUNT || !g_windows[handle].exists)
    return;
  compositor_event ev = {.type = CET_WINDOW_PRESENT};
  ev.present_window.handle = handle;
  queue_push(&ev);
}

int compositor_post_event(window_handle handle, window_event *ev) {
  if (handle >= WINDOW_MAX_COUNT || !g_windows[handle].exists)
    return -1;
  kernel_window *w = &g_windows[handle];
  int next = (w->ev_head + 1) % WINDOW_EVENT_QUEUE;
  if (next == w->ev_tail)
    return -1;
  w->events[w->ev_head] = *ev;
  w->ev_head = next;
  return 0;
}

int compositor_window_poll_event(window_handle handle, window_event *ev) {
  if (handle >= WINDOW_MAX_COUNT || !g_windows[handle].exists)
    return 0;
  kernel_window *w = &g_windows[handle];
  if (w->ev_head == w->ev_tail)
    return 0;
  *ev = w->events[w->ev_tail];
  w->ev_tail = (w->ev_tail + 1) % WINDOW_EVENT_QUEUE;
  return 1;
}

void compositor_get_framebuffer(window_handle handle, fb_info *fb) {
  if (handle >= WINDOW_MAX_COUNT || !g_windows[handle].exists)
    return;
  kernel_window *w = &g_windows[handle];
  fb->ptr   = (uint *)w->client_vaddr;
  fb->width = w->width;
  fb->height = w->height;
  fb->pitch = w->width * 4;
  fb->dirty_region.w = 0;
}