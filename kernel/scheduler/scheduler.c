// #include "scheduler.h"
// #include "task.h"
//
// extern void context_switch(cpu_context_t *old_ctx, cpu_context_t *new_ctx);
//
// static task_t *g_current = 0;
// static task_t *g_task_list = 0;
// static int g_next_pid = 1;
//
// typedef void (*task_entry_t)(void);
//
//__attribute__((noreturn)) void task_exit(void) {
//   g_current->state = TASK_DEAD;
//
//   // schedule();
//
//   // panic("task_exit returned");
// }
//
//__attribute__((noreturn)) void task_bootstrap(task_entry_t entry) {
//   entry();
//
//   // If task function returns, kill task
//   task_exit();
//
//   for (;;) {
//     __asm__ volatile("cli; hlt");
//   }
// }
//
// static task_t *scheduler_pick_next(void) {
//   if (!g_current) {
//     return g_task_list;
//   }
//
//   task_t *t = g_current->next;
//
//   while (t != g_current) {
//     if (t->state == TASK_READY) {
//       return t;
//     }
//     t = t->next;
//   }
//
//   if (g_current->state == TASK_READY || g_current->state == TASK_RUNNING) {
//     return g_current;
//   }
//
//   return 0;
// }
//
// void scheduler_init() {}
// void scheduler_add(task_t *task) {
//   if (!g_task_list) {
//     g_task_list = task;
//     task->next = task;
//     return;
//   }
//
//   task->next = g_task_list->next;
//   g_task_list->next = task;
// }
// void scheduler_run() {
//   task_t *next = scheduler_pick_next();
//   if (!next) {
//     return;
//   }
//
//   if (!g_current) {
//     g_current = next;
//     g_current->state = TASK_RUNNING;
//
//     cpu_context_t dummy = {0};
//     context_switch(&dummy, &g_current->context);
//     return;
//   }
//
//   if (next == g_current) {
//     return;
//   }
//
//   task_t *prev = g_current;
//
//   if (prev->state == TASK_RUNNING) {
//     prev->state = TASK_READY;
//   }
//
//   g_current = next;
//   g_current->state = TASK_RUNNING;
//
//   context_switch(&prev->context, &g_current->context);
// }