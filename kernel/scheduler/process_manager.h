#pragma once
#include "process_info.h"
#include "scheduler/process.h"

void process_manager_init();
pid process_exec(const char *path, int std_in, int std_out);
exit_code process_wait(pid pid);
int process_list(process_info *buf, int count);