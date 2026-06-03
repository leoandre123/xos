#include "perf.h"
#include "io/serial.h"

#ifdef KERNEL_PERF

extern perf_counter __perf_counters_start[];
extern perf_counter __perf_counters_end[];

perf_counter g_perf_user_time _PERF_CTR = {"[user time]", 0, 0, ~(ulong)0, 0};
perf_counter g_perf_kernel_time _PERF_CTR = {"[kernel time]", 0, 0, ~(ulong)0, 0};
perf_counter g_perf_irq_time _PERF_CTR = {"[irq time]", 0, 0, ~(ulong)0, 0};

ulong g_idle_time = 0;
ulong g_idle_start = 0;

#define PERF_MAX_COUNTERS 128
#define PERF_SAMPLE_MAX   512
#define PERF_TOP_N        16

static ulong g_perf_epoch = 0;

static ulong g_samples[PERF_SAMPLE_MAX];
static uint g_sample_n = 0;
static uint g_sample_head = 0;

void perf_add_sample(ulong rip) {
  g_samples[g_sample_head] = rip;
  g_sample_head = (g_sample_head + 1) % PERF_SAMPLE_MAX;
  if (g_sample_n < PERF_SAMPLE_MAX)
    g_sample_n++;
}

static inline ulong perf_rdtsc(void) {
  uint lo, hi;
  __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return ((ulong)hi << 32) | lo;
}

void perf_reset(void) {
  g_perf_epoch = perf_rdtsc();
  for (perf_counter *c = __perf_counters_start; c < __perf_counters_end; c++) {
    c->total_cycles = 0;
    c->call_count = 0;
    c->min_cycles = ~(ulong)0;
    c->max_cycles = 0;
  }
  g_sample_n = 0;
  g_sample_head = 0;
  g_idle_start = g_idle_start ? perf_rdtsc() : 0;
  g_idle_time = 0;
}

void perf_report(void) {
  PERF_SCOPE("perf_report");
  perf_counter *base = __perf_counters_start;
  ulong n = (ulong)(__perf_counters_end - __perf_counters_start);
  if (n > PERF_MAX_COUNTERS)
    n = PERF_MAX_COUNTERS;

  // Collect only counters that have been hit at least once
  perf_counter *sorted[PERF_MAX_COUNTERS];
  ulong m = 0;
  for (ulong i = 0; i < n; i++)
    if (base[i].call_count > 0)
      sorted[m++] = &base[i];

  // Selection sort: highest total_cycles first
  for (ulong i = 0; i + 1 < m; i++) {
    ulong best = i;
    for (ulong j = i + 1; j < m; j++)
      if (sorted[j]->total_cycles > sorted[best]->total_cycles)
        best = j;
    perf_counter *tmp = sorted[i];
    sorted[i] = sorted[best];
    sorted[best] = tmp;
  }

  ulong elapsed = g_perf_epoch ? perf_rdtsc() - g_perf_epoch : 0;

  serial_printf("\n--- perf report (%u counters, %u elapsed cycles) ---\n", m, elapsed);
  serial_printf("\n--- idle time   (%u cycles  -              %u%%) ---\n", g_idle_time, elapsed ? (g_idle_time * 100) / elapsed : 0);
  serial_printf("|name                           tot_cyc        calls    avg_cyc    min_cyc    max_cyc      %%|\n");
  serial_printf("|------------------------------ -------------- -------- ---------- ---------- ---------- ----|\n");
  for (ulong i = 0; i < m; i++) {
    perf_counter *c = sorted[i];
    ulong avg = c->total_cycles / c->call_count;
    ulong pct = elapsed ? (c->total_cycles * 1000) / elapsed : 0;
    serial_printf("|%30s %14u %8u %10u %10u %10u %3u.%u|\n",
                  c->name, c->total_cycles, c->call_count, avg,
                  c->min_cycles, c->max_cycles, pct / 10, pct % 10);
  }
  serial_printf("|------------------------------ -------------- -------- ---------- ---------- ---------- ----|\n");

  // Statistical sample profile
  if (g_sample_n == 0)
    return;

  typedef struct {
    ulong rip;
    uint count;
  } rip_count_t;
  static rip_count_t uniq[PERF_SAMPLE_MAX];
  uint nu = 0;
  for (uint i = 0; i < g_sample_n; i++) {
    ulong r = g_samples[i];
    uint j;
    for (j = 0; j < nu; j++)
      if (uniq[j].rip == r) {
        uniq[j].count++;
        break;
      }
    if (j == nu) {
      uniq[nu].rip = r;
      uniq[nu].count = 1;
      nu++;
    }
  }
  // selection sort: highest count first, top PERF_TOP_N
  uint show = nu < PERF_TOP_N ? nu : PERF_TOP_N;
  for (uint i = 0; i < show; i++) {
    uint best = i;
    for (uint j = i + 1; j < nu; j++)
      if (uniq[j].count > uniq[best].count)
        best = j;
    rip_count_t tmp = uniq[i];
    uniq[i] = uniq[best];
    uniq[best] = tmp;
  }

  serial_printf("\n--- sample profile (%u samples) ---\n", g_sample_n);
  serial_printf("| count |    %% | rip                |\n");
  serial_printf("|-------|------|--------------------||\n");
  for (uint i = 0; i < show; i++) {
    uint pct10 = (uniq[i].count * 1000) / g_sample_n;
    serial_printf("| %5u | %2u.%u%% | 0x%016x |\n",
                  uniq[i].count, pct10 / 10, pct10 % 10, uniq[i].rip);
  }
  serial_printf("resolve: .venv/bin/python tools/pagefault.py <rip>\n");
}

#else // !KERNEL_PERF

void perf_report(void) {}
void perf_reset(void) {}
void perf_add_sample(ulong rip) { (void)rip; }

#endif // KERNEL_PERF
