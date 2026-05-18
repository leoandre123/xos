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
#define SYS_READ_MOUSE    14 // non-blocking; returns (1<<48)|(buttons<<32)|(y<<16)|x or 0
#define SYS_WRITE_HEX     20

/* MEMORY 30-40*/
#define SYS_ALLOC 30 // arg1=size in bytes; maps anon pages, returns user vaddr or 0
#define SYS_FREE  31

/* TIME 40- */
#define SYS_UNIX_TIME        40
#define SYS_UNIX_TIME_MILLIS 41
#define SYS_VBLANK_WAIT      42 // blocking wating for vsync, no args

/* PROCESSES 60-79 */
#define SYS_PROCESS_EXEC 60
#define SYS_PROCESS_LIST 61 // arg1=buf, arg2=max_count

/* FILES 80-99*/
#define SYS_FILE_OPEN    80 // arg1=path, arg2=option returns handle
#define SYS_FILE_CLOSE   81 // arg1=handle
#define SYS_FILE_READ    82 // arg1=handle, arg2=buf, arg3=count
#define SYS_FILE_SIZE    83 // arg1=handle
#define SYS_FILE_READDIR 90 // arg1=path, arg2=buf, arg3=max_count, returns count read

/* NETWORKING 100-119 */
#define SYS_SOCKET_CONNECT 100
#define SYS_SOCKET_LISTEN  101
#define SYS_SOCKET_ACCEPT  102
#define SYS_SOCKET_SEND    103
#define SYS_SOCKET_RECEIVE 104
#define SYS_SOCKET_CLOSE   105
#define SYS_SOCKET_UDP     106 // arg1=remote_addr, arg2=remote_port, arg3=local_port; returns socket handle
#define SYS_NET_GET_MAC    110 // arg1=ubyte[6] buf; fills MAC address
#define SYS_NET_SET_IP     111 // arg1=ipv4_addr.value; sets kernel IP after DHCP

/* WINDOW MANAGER 120-129 */
#define SYS_WM_REGISTER 120
#define SYS_WM_POLL     121

/* WINDOW 130-139*/
#define SYS_WINDOW_CREATE      130
#define SYS_WINDOW_POLL        131
#define SYS_WINDOW_PRESENT     132
#define SYS_WINDOW_POST_EVENT  133
#define SYS_WINDOW_FRAMEBUFFER 134

/* STATS 140-149 */
#define SYS_STATS_MEMORY 140 // arg1=&mem_info
#define SYS_STATS_CPU    141 // arg1=&cpu_info