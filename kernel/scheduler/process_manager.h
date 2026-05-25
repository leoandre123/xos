#pragma once
#include "noreturn.h"
#include "process_info.h"
#include "scheduler/process.h"
#include "thread.h"

void process_manager_init();
process *find_process(pid pid);
pid process_exec(const char *path, int std_in, int std_out, int argc, const char **argv);
exit_code process_wait(pid pid);
int process_list(process_info *buf, int count);

NORETURN process_exit(exit_code code); // Exits the current process
void process_kill(process *p);         // Kills all threads in process
void process_delete(process *p);       // Deletes all threads in p and itself

// THREADS

thread_handle process_spawn_thread(void(*entry), ulong arg);
void process_kill_thread(thread_handle h);
ulong process_join_thread(thread_handle h);
NORETURN process_exit_thread(ulong ret_val);