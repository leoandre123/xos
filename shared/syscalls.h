#pragma once

// Number used by user programs to write a string to serial
#define SYS_WRITE         0
#define SYS_EXIT          1
#define SYS_WRITE_CONSOLE 2
#define SYS_READ_KEY      3  // blocks until key; returns (char<<32)|keycode
#define SYS_EXEC          4  // arg1=path, arg2=stdin_fd, arg3=stdout_fd; returns pid or -1
#define SYS_WAIT          5  // arg1=pid; blocks until that pid exits
#define SYS_PIPE          6  // returns (write_fd<<32)|read_fd
#define SYS_READ_FD       7  // arg1=fd, arg2=buf, arg3=len; returns bytes read
#define SYS_WRITE_FD      8  // arg1=fd, arg2=buf, arg3=len; returns bytes written
#define SYS_MAP_FB        9  // arg1=fb_info*; maps framebuffer into task, fills struct
#define SYS_PIPE_AVAIL    10 // arg1=fd; returns bytes available without blocking
#define SYS_READ_KEY_NB   11 // non-blocking; returns (char<<32)|code or 0 if no key
#define SYS_YIELD         12 // yield CPU to scheduler
#define SYS_ALLOC         13 // arg1=size in bytes; maps anon pages, returns user vaddr or 0
#define SYS_READ_MOUSE    14 // non-blocking; returns (1<<48)|(buttons<<32)|(y<<16)|x or 0
#define SYS_WRITE_HEX     20
#define SYS_TIME          40
/* PROCESSES 60-79 */

/* FILES 80-99*/
#define SYS_FILE_OPEN    80 // arg1=path, arg2=option returns handle
#define SYS_FILE_CLOSE   81 // arg1=handle
#define SYS_FILE_READDIR 90 // arg1=path, arg2=buf, arg3=max_count, returns count read

/* NETWORKING 100-119 */
#define SYS_SOCKET_CONNECT 100
#define SYS_SOCKET_LISTEN  101
#define SYS_SOCKET_ACCEPT  102
#define SYS_SOCKET_SEND    103
#define SYS_SOCKET_RECEIVE 104
#define SYS_SOCKET_CLOSE   105

/* COMPOSITOR 120-129 */
#define SYS_COMPOSITOR_REGISTER 120
#define SYS_COMPOSITOR_POLL     121

/* WINDOW 130- */
#define SYS_WINDOW_CREATE      130
#define SYS_WINDOW_POLL        131
#define SYS_WINDOW_PRESENT     132
#define SYS_WINDOW_POST_EVENT  133
#define SYS_WINDOW_FRAMEBUFFER 134
