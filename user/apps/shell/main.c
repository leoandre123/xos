#include "../../lib/syscall.h"

#define bool _Bool
#define true 1
#define false 0

#define MAX_CMD 256
#define MAX_ARGS 8
#define MAX_ARG_LENGTH 64

#define MAX_PARTS 64
#define MAX_PART_LENGTH 64

static char current_dir[256] = "/";

static unsigned int slen(const char *s) {
  unsigned int n = 0;
  while (s[n])
    n++;
  return n;
}

// Write to stdout fd (1). All shell output goes through here.
static void out(const char *s) { sys_write_fd(1, s, slen(s)); }

// Read one character from stdin fd (0). Blocks until a byte arrives.
static char in(void) {
  char c;
  sys_read_fd(0, &c, 1);
  return c;
}

static int strcmp(const char *a, const char *b) {
  while (*a && *a == *b) {
    a++;
    b++;
  }
  return *a - *b;
}

static int strcopy(char *dst, const char *src, int count, bool add_null) {
  int i = 0;
  while (src[i] != '\0' && (count == -1 || i < count)) {
    dst[i] = src[i];
    i++;
  }
  if (add_null)
    dst[i] = '\0';

  return i;
}

static void push_part(char parts[MAX_PARTS][MAX_PART_LENGTH], int *part_count,
                      const char *start, uint len) {
  if (len == 0 || len >= MAX_PART_LENGTH || *part_count >= MAX_PARTS) {
    return;
  }

  if (len == 1 && start[0] == '.') {
    return;
  }

  if (len == 2 && start[0] == '.' && start[1] == '.') {
    if (*part_count > 0) {
      (*part_count)--;
    }
    return;
  }

  strcopy(parts[*part_count], start, len, true);
  (*part_count)++;
}

static void parse_into_parts(const char *path,
                             char parts[MAX_PARTS][MAX_PART_LENGTH],
                             int *part_count) {
  const char *p = path;

  while (*p) {
    while (*p == '/') {
      p++;
    }

    const char *start = p;

    while (*p && *p != '/') {
      p++;
    }

    uint len = (uint)(p - start);
    push_part(parts, part_count, start, len);
  }
}

static void cmd_cd(char args[MAX_ARGS][MAX_ARG_LENGTH], int arg_count) {
  if (arg_count < 2) {
    out("Incorrect usage: \n");
    return;
  }

  sys_write("CD 1");

  static char parts[MAX_PARTS][MAX_PART_LENGTH];
  int part_count = 0;

  if (args[1][0] != '/') {
    parse_into_parts(current_dir, parts, &part_count);
  }
  sys_write("CD 2");
  parse_into_parts(args[1], parts, &part_count);

  sys_write("CD 3");
  char new_dir[256] = "/";
  char *dir_ptr = &new_dir[1];

  sys_write_hex(part_count);
  for (int i = 0; i < part_count; i++) {
    dir_ptr += strcopy(dir_ptr, parts[i], -1, false);
    if (i != part_count - 1)
      *dir_ptr++ = '/';
  }
  sys_write("CD 4");
  if (true) {
    strcopy(current_dir, new_dir, -1, true);
  } else {
    out("Cannot resolve path: ");
    out(new_dir);
    out("\n");
  }
}

static int parse_command(char *cmd, char args[MAX_ARGS][MAX_ARG_LENGTH]) {
  int ci = 0;
  int a = 0;

  while (cmd[ci] != '\0' && a < MAX_ARGS) {
    while (cmd[ci] == ' ') {
      ci++;
    }

    if (cmd[ci] == '\0') {
      break;
    }

    int ai = 0;
    while (cmd[ci] != '\0' && cmd[ci] != ' ') {
      if (ai < MAX_ARG_LENGTH - 1) {
        args[a][ai++] = cmd[ci];
      }
      ci++;
    }

    args[a][ai] = '\0';
    a++;
  }

  return a;
}
static void run(char *cmd, char args[MAX_ARGS][MAX_ARG_LENGTH], int arg_count) {
  sys_write("Shell run: ");
  sys_write(cmd);
  sys_write("\n");
  for (int i = 0; i < arg_count; i++) {
    sys_write(args[i]);
    sys_write("\n");
  }
  if (cmd[0] == '\0')
    return;

  if (strcmp(cmd, "exit") == 0) {
    out("Bye!\n");
    sys_exit();
  }
  if (strcmp(args[0], "cd") == 0) {
    cmd_cd(args, arg_count);
    return;
  }

  char path[MAX_CMD + 2];
  int i = 0;
  if (cmd[0] != '/')
    path[i++] = '/';
  for (int j = 0; cmd[j]; j++)
    path[i++] = cmd[j];
  path[i] = '\0';

  // Child inherits shell's stdin (fd 0) and stdout (fd 1).
  // The child writes to fd 1 → same pipe the terminal reads → appears on
  // screen.
  int pid = sys_exec_fds(path, 0, 1);
  if (pid < 0) {
    path[i++] = '.';
    path[i++] = 'e';
    path[i++] = 'l';
    path[i++] = 'f';
    path[i++] = '\0';
    pid = sys_exec_fds(path, 0, 1);
  }
  if (pid < 0) {
    out("Unknown command: ");
    out(cmd);
    out("\n");
    return;
  }
  sys_wait(pid);
}

void _start(void) {
  sys_write("Hello from shell!\n");
  out("LeOS Shell\n");

  char buf[MAX_CMD];

  char args[MAX_ARGS][MAX_ARG_LENGTH];

  while (1) {
    out(current_dir);
    out("> ");

    int len = 0;
    while (1) {
      sys_write("Waiting...");
      ulong rsp;
      asm volatile("mov %%rsp, %0" : "=r"(rsp));
      sys_write_hex(rsp);
      char c = in();
      sys_write("KEY!\n");

      if (c == '\n' || c == '\r') {
        out("\n");
        break;
      }
      if (c == '\b' || c == 127) {
        if (len > 0) {
          len--;
          out("\b \b");
        }
        continue;
      }
      if (len < MAX_CMD - 1) {
        buf[len++] = c;
        char s[2] = {c, '\0'};
        out(s);
      }
    }
    buf[len] = '\0';

    int arg_count = parse_command(buf, args);
    run(buf, args, arg_count);
  }
}
