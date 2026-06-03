#pragma once
#include "types.h"

typedef struct {
  const char *name;
  ulong total_cycles;
  ulong call_count;
  ulong min_cycles;
  ulong max_cycles;
} perf_counter;

// Always available regardless of KERNEL_PERF
void perf_report(void);
void perf_reset(void);
void perf_add_sample(ulong rip);

#ifdef KERNEL_PERF

// Marks a static counter as belonging to the .perf_counters linker section.
// aligned(8) prevents GCC from requesting 32-byte section alignment, which
// would create a gap between __perf_counters_start and the first struct.
#define _PERF_CTR __attribute__((section(".perf_counters"), used, aligned(8)))

static inline ulong _rdtsc(void) {
  uint lo, hi;
  __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return ((ulong)hi << 32) | lo;
}

static inline void _perf_record(perf_counter *c, ulong cycles) {
  c->total_cycles += cycles;
  c->call_count++;
  if (cycles < c->min_cycles)
    c->min_cycles = cycles;
  if (cycles > c->max_cycles)
    c->max_cycles = cycles;
}

// __COUNTER__ increments each time it appears in source. The helper macros
// receive it as 'n' so it is frozen to one value for both the struct name
// and the local variable that references it.

#define _PERF_BEGIN_IMPL(n, label)                                       \
  static perf_counter _pc_##n _PERF_CTR = {(label), 0, 0, ~(ulong)0, 0}; \
  perf_counter *_perf_cur = &_pc_##n;                                    \
  ulong _perf_t0 = _rdtsc()

// PERF_BEGIN("label") / PERF_END()
//
//   PERF_BEGIN("fat32/read");
//   fat32_read_sectors(...);
//   PERF_END();
#define PERF_BEGIN(label) _PERF_BEGIN_IMPL(__COUNTER__, label)
#define PERF_END()        _perf_record(_perf_cur, _rdtsc() - _perf_t0)

// PERF_SCOPE("label")
// Records cycles from this line to the end of the enclosing C scope.
// Uses GCC cleanup attribute — works with early returns and nested scopes.
//
//   void syscall_dispatch(...) {
//       PERF_SCOPE("syscall/dispatch");
//       ...
//   }  // <-- automatically recorded here
typedef struct {
  ulong t0;
  perf_counter *ctr;
} _perf_guard_t;

static inline void _perf_guard_exit(_perf_guard_t *g) {
  _perf_record(g->ctr, _rdtsc() - g->t0);
}

#define _PERF_SCOPE_IMPL(n, label)                                       \
  static perf_counter _pc_##n _PERF_CTR = {(label), 0, 0, ~(ulong)0, 0}; \
  __attribute__((cleanup(_perf_guard_exit))) _perf_guard_t _pg_##n = {_rdtsc(), &_pc_##n}

#define PERF_SCOPE(label) _PERF_SCOPE_IMPL(__COUNTER__, label)

// ── User / kernel time ────────────────────────────────────────────────────────
// Call PERF_MODE_ENTER_KERNEL(task) at syscall entry and
// PERF_MODE_ENTER_USER(task) at syscall exit (sysret path).
// Call PERF_MODE_SWITCH(prev, next) inside schedule() when swapping tasks.
// All three are no-ops when KERNEL_PERF is off.

extern perf_counter g_perf_user_time;
extern perf_counter g_perf_kernel_time;
extern ulong g_idle_time;
extern ulong g_idle_start;

// Charge [tsc - t->perf_last_tsc] to the appropriate counter, then reset.
#define _PERF_CHARGE(t, ctr)                           \
  do {                                                 \
    ulong _now = _rdtsc();                             \
    if ((t)->perf_last_tsc)                            \
      _perf_record(&(ctr), _now - (t)->perf_last_tsc); \
    (t)->perf_last_tsc = _now;                         \
  } while (0)

// Task crosses from user → kernel (syscall entry).
#define PERF_MODE_ENTER_KERNEL(t)      \
  do {                                 \
    _PERF_CHARGE(t, g_perf_user_time); \
    (t)->perf_in_syscall = true;       \
  } while (0)

// Task crosses from kernel → user (sysret / syscall return).
#define PERF_MODE_ENTER_USER(t)          \
  do {                                   \
    _PERF_CHARGE(t, g_perf_kernel_time); \
    (t)->perf_in_syscall = false;        \
  } while (0)

// Called in schedule(): charge prev's current slice, zero next's clock.
// The caller must arm next->perf_last_tsc after context_switch returns so
// that vmm_switch / context_switch overhead running on prev's stack is not
// attributed to next's kernel time.
#define PERF_MODE_SWITCH(prev, next)            \
  do {                                          \
    if (prev) {                                 \
      if ((prev)->perf_in_irq)                  \
        _PERF_CHARGE(prev, g_perf_irq_time);    \
      else if ((prev)->perf_in_syscall)         \
        _PERF_CHARGE(prev, g_perf_kernel_time); \
      else                                      \
        _PERF_CHARGE(prev, g_perf_user_time);   \
      (prev)->perf_last_tsc = 0;                \
    }                                           \
    (next)->perf_last_tsc = 0;                  \
  } while (0)

// Call immediately after context_switch returns to start the resumed task's clock.
#define PERF_MODE_RESUME(t)          \
  do {                               \
    if (t)                           \
      (t)->perf_last_tsc = _rdtsc(); \
  } while (0)

#define PERF_ENTER_IDLE() g_idle_start = _rdtsc()
#define PERF_EXIT_IDLE()  g_idle_time += (_rdtsc() - g_idle_start)
// PERF_MODE_SYSCALL_SCOPE(t)
// Drop this at the top of syscall_dispatch (after getting the current task).
// Charges elapsed time as user on entry, then automatically charges kernel
// time and restores user mode at function exit — handles all return paths.
//
//   task *t = scheduler_current();
//   PERF_MODE_SYSCALL_SCOPE(t);
typedef struct {
  ulong *last_tsc;
  bool *in_syscall;
} _perf_mode_guard_t;

static inline void _perf_mode_guard_exit(_perf_mode_guard_t *g) {
  ulong _now = _rdtsc();
  if (*g->last_tsc)
    _perf_record(&g_perf_kernel_time, _now - *g->last_tsc);
  *g->last_tsc = _now;
  *g->in_syscall = false;
}

#define PERF_MODE_SYSCALL_SCOPE(t)                                           \
  PERF_MODE_ENTER_KERNEL(t);                                                 \
  __attribute__((cleanup(_perf_mode_guard_exit))) _perf_mode_guard_t _pmsg = \
      {&(t)->perf_last_tsc, &(t)->perf_in_syscall}

// PERF_IRQ_SCOPE(t)
// Place at the very top of an interrupt handler (t = scheduler_current()).
// 1. Charges any in-flight user/kernel slice for t up to the IRQ entry point.
// 2. Times the handler itself.
// 3. Re-arms t's clock on exit so the next mode boundary is accurate.
//
//   static void timer_handler() {
//       PERF_IRQ_SCOPE(scheduler_current());
//       ...
//   }

extern perf_counter g_perf_irq_time;

typedef struct {
  ulong *last_tsc;
  bool *in_irq;
} _perf_irq_guard_t;

static inline void _perf_irq_guard_exit(_perf_irq_guard_t *g) {
  ulong _now = _rdtsc();
  if (g->last_tsc && *g->last_tsc)
    _perf_record(&g_perf_irq_time, _now - *g->last_tsc);
  if (g->last_tsc)
    *g->last_tsc = _now;
  if (g->in_irq)
    *g->in_irq = false;
}

#define PERF_IRQ_SCOPE(t)                                                  \
  ulong _pirg_t0 = _rdtsc();                                               \
  do {                                                                     \
    if ((t) && (t)->perf_last_tsc) {                                       \
      ulong _e = _pirg_t0 - (t)->perf_last_tsc;                            \
      if ((t)->perf_in_syscall)                                            \
        _perf_record(&g_perf_kernel_time, _e);                             \
      else                                                                 \
        _perf_record(&g_perf_user_time, _e);                               \
    }                                                                      \
    if (t) {                                                               \
      (t)->perf_last_tsc = _pirg_t0;                                       \
      (t)->perf_in_irq = true;                                             \
    }                                                                      \
  } while (0);                                                             \
  __attribute__((cleanup(_perf_irq_guard_exit))) _perf_irq_guard_t _pirg = \
      {(t) ? &(t)->perf_last_tsc : 0, (t) ? &(t)->perf_in_irq : 0}

#else // !KERNEL_PERF — zero-cost at compile time

#define PERF_BEGIN(label)            ((void)0)
#define PERF_END()                   ((void)0)
#define PERF_SCOPE(label)            ((void)0)
#define PERF_MODE_ENTER_KERNEL(t)    ((void)0)
#define PERF_MODE_ENTER_USER(t)      ((void)0)
#define PERF_MODE_SWITCH(prev, next) ((void)0)
#define PERF_MODE_RESUME(t)          ((void)(t))
#define PERF_MODE_SYSCALL_SCOPE(t)   ((void)(t))
#define PERF_IRQ_SCOPE(t)            ((void)(t))
#define PERF_ENTER_IDLE()            ((void)0)
#define PERF_EXIT_IDLE()             ((void)0)

#endif // KERNEL_PERF
