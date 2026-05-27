
#include "fs/file.h"
#include "string.h"
#include "syscall.h"
#include "time.h"
#include "utils.h"
#include <stdio.h>

bool strcmp(const char *str1, const char *str2, int len) {
  for (int i = 0; i < len; i++) {
    if (str1[i] != str2[i]) {
      return false;
    }
  }
  return true;
}

int next_line(const char *buf, int buf_len, int current) {
  int i;
  for (i = current; i < buf_len; i++) {
    if (buf[i] == '\n') {
      return current + 1;
    }
  }
  return i + 1;
}
bool parse_cfg_key(const char *buf, int buf_len, const char *key, char *out) {
  int key_len = strlen(key);
  int line_index = 0;
  while (line_index != -1) {
    if (key_len + 2 > buf_len - line_index) {
      return false;
    }
    if (str_starts_with(&buf[line_index], key)) {
      if (buf[line_index + key_len] == '=') {
        const char *to_out = &buf[line_index + key_len + 1];
        while (*to_out != '\n' && *to_out != '\0') {
          *out++ = *to_out++;
        }
        *out = '\0';
        return true;
      }
    }
    line_index = next_line(buf, buf_len, line_index);
  }
  return false;
}

int main() {
  file_handle init_h = file_open("/boot/init.cfg");

  char buf[257];
  int len = file_read(init_h, buf, 255);
  buf[len] = '\0';

  char out[32];
  parse_cfg_key(buf, len, "DISPLAY_OWNER", out);
  sys_write(buf);
  sys_write(out);

  sys_exec("/sys/programs/dafne.elf");
  sys_exec("/sys/programs/dhcp.elf");

  while (true) {
    sleep(600000);
  }
}