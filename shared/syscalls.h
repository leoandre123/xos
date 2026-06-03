#pragma once

// Number used by user programs to write a string to serial
#define SYS_WRITE         0
#define SYS_EXIT          1
#define SYS_WRITE_CONSOLE 2
#define SYS_READ_KEY      3 // blocks until key; returns (char<<32)|keycode
#define SYS_EXEC          4 // arg1=path, arg2=stdin_fd, arg3=stdout_fd; returns pid or -1
#define SYS_WAIT          5 // arg1=pid; blocks until that pid exits

#define SYS_READ_FD  7 // arg1=fd, arg2=buf, arg3=len; returns bytes read
#define SYS_WRITE_FD 8 // arg1=fd, arg2=buf, arg3=len; returns bytes written
#define SYS_MAP_FB   9 // arg1=fb_info*; maps framebuffer into task, fills struct

#define SYS_READ_KEY_NB 11 // non-blocking; returns (char<<32)|code or 0 if no key
#define SYS_YIELD       12 // yield CPU to scheduler
#define SYS_READ_MOUSE  14 // non-blocking; returns (1<<48)|(buttons<<32)|(y<<16)|x or 0

/* MEMORY 30-39*/
#define SYS_ALLOC        30 // arg1=size in bytes; maps anon pages, returns user vaddr or 0
#define SYS_FREE         31
#define SYS_ALLOC_SHARED 32 // arg1=size, arg2=ch handle, arg3=[OUT]client_vaddr

/* TIME 40- */
#define SYS_UNIX_TIME        40
#define SYS_UNIX_TIME_MILLIS 41
#define SYS_SLEEP            43 // blocks for arg1 ms

/* PROCESSES 60-69 */
#define SYS_PROCESS_EXEC 60
#define SYS_PROCESS_LIST 61 // arg1=buf, arg2=max_count
#define SYS_PROCESS_PATH 62 // arg1=pid, self if 0, arg2 buf, arg3 len

/* THREADING 70-79 */
#define SYS_THREAD      70 // arg1=entry, returns handle
#define SYS_THREAD_JOIN 71 // arg1=handle
#define SYS_THREAD_KILL 72 // arg1=handle
#define SYS_THREAD_EXIT 73 // arg1=return value

/* FILES 80-99*/
#define SYS_FILE_OPEN    80 // arg1=path, arg2=option returns handle
#define SYS_FILE_CLOSE   81 // arg1=handle
#define SYS_FILE_READ    82 // arg1=handle, arg2=buf, arg3=count
#define SYS_FILE_SIZE    83 // arg1=handle
#define SYS_FILE_READDIR 90 // arg1=path, arg2=buf, arg3=max_count, returns count read

/* NETWORKING 100-119 */
#define SYS_SOCKET            100
#define SYS_SOCKET_CLOSE      101
#define SYS_SOCKET_BIND       102
#define SYS_SOCKET_LISTEN     103
#define SYS_SOCKET_ACCEPT     104
#define SYS_SOCKET_CONNECT    105
#define SYS_SOCKET_SEND       106
#define SYS_SOCKET_RECEIVE    107
#define SYS_SOCKET_BIND_NIC   108
#define SYS_SOCKET_RECEIVE_NB 109

#define SYS_NET_GET_MAC  110 // arg1=ubyte[6] buf; fills MAC address
#define SYS_NET_SET_IP   111 // arg1=ipv4_addr.value; sets kernel IP after DHCP
#define SYS_NET_NICS     112 // arg1=nic_info*, arg2=len
#define SYS_NET_CONF_NIC 113 // arg1=nic_id, arg2=field, arg3=value
#define SYS_NET_ROUTES   114 // arg1=route_info*, arg2=len

/* IPC 120-139 */
#define SYS_PIPE                 120 // returns (write_fd<<32)|read_fd
#define SYS_PIPE_AVAIL           121 // arg1=fd; returns bytes available without blocking
#define SYS_IPC_SERVER           122 // arg1=name, returns srv handle
#define SYS_IPC_SERVER_ACCEPT    123 // arg1=srv handle, returns pipe handle
#define SYS_IPC_SERVER_CONNECT   124 // arg1=name, return srv handle
#define SYS_IPC_SEND             125
#define SYS_IPC_RECV             126
#define SYS_IPC_SERVER_ACCEPT_NB 127
#define SYS_IPC_RECV_NB          128
#define SYS_IPC_CHANNEL_CLOSE    129
#define SYS_IPC_PID              130

/* STATS 140-149 */
#define SYS_STATS_MEMORY 140 // arg1=&mem_info
#define SYS_STATS_CPU    141 // arg1=&cpu_info
#define SYS_INFO         142 // arg1=type, arg2=struct_ptr

/* POWER 150-159 */
#define SYS_BATTERY_INFO 150 // arg1=index, arg2=&battery_info; returns 1 if present else 0