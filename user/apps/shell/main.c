#include "../../lib/syscall.h"

#define MAX_CMD 256

static void print(const char *s) { sys_print(s); }
static void println(const char *s) {
  sys_print(s);
  sys_print("\n");
}

static int strcmp(const char *a, const char *b) {
  while (*a && *a == *b) {
    a++;
    b++;
  }
  return *a - *b;
}

static void run(char *cmd) {
  if (cmd[0] == '\0')
    return;

  if (strcmp(cmd, "exit") == 0) {
    println("Bye!");
    sys_exit();
  }

  // Try to exec as a file path
  // Prepend / if not already absolute
  char path[MAX_CMD + 2];
  int i = 0;
  if (cmd[0] != '/') {
    path[i++] = '/';
  }
  for (int j = 0; cmd[j]; j++)
    path[i++] = cmd[j];
  path[i] = '\0';

  int pid = sys_exec(path);
  if (pid < 0) {
    print("Unknown command: ");
    println(cmd);
    return;
  }
  sys_wait(pid);
}

void _start(void) {
  println("LeOS Shell");
  println("----------");

  char buf[MAX_CMD];

  while (1) {
    sys_print("> ");

    int len = 0;
    while (1) {
      char c = sys_read_char();
      if (!c)
        continue;

      if (c == '\n') {
        sys_print("\n");
        break;
      }
      if (c == '\b' || c == 127) {
        if (len > 0) {
          len--;
          sys_print("\b \b");
        }
        continue;
      }

      if (len < MAX_CMD - 1) {
        buf[len++] = c;
        char s[2] = {c, '\0'};
        sys_print(s);
      }
    }
    buf[len] = '\0';
    run(buf);
  }
}
