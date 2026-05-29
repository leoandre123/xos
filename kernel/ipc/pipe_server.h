#pragma once
#include "ipc/pipe.h"

typedef int pipe_server_handle;

pipe_server_handle pipe_server_register(const char *identifier);
pipe *pipe_server_accept(pipe_server_handle h);
pipe *pipe_server_join(const char *identifier);