
#include "route_info.h"
#include "string.h"
#include "syscall.h"
#include "syscalls.h"
#include "threads.h"
#include "time.h"
#include <stdio.h>

static inline int sys_net_routes(route_info *infos, int count) {
  return syscall(SYS_NET_ROUTES, (ulong)infos, count, 0);
}

void print_all_routes() {
  route_info routes[10];
  int count = sys_net_routes(&routes[0], 10);
  printf("|  Destination  |    Netmask    |    Gateway    |NIC|Metric|\n");
  for (int i = 0; i < count; i++) {
    route_info *r = &routes[i];
    printf(" %3d.%3d.%3d.%3d|%3d.%3d.%3d.%3d|%3d.%3d.%3d.%3d|%3d|%6d|\n",
           IPV4_SPILL(r->destination), IPV4_SPILL(r->netmask),
           IPV4_SPILL(r->gateway), (long)(r->nic_id & 0xFFF % 1000), r->metric);
  }
}

char print_loop(char c) {
  for (int i = 0; i < 100; i++) {
    printf("%c", c);
    sleep(1);
  }
  return c;
}

int main(int argc, char *argv[]) {

  if (argc >= 2 && strcmp(argv[1], "test") == 0) {
    thread_handle t1 = thread_spawn(print_loop, 'a');
    thread_handle t2 = thread_spawn(print_loop, 'b');
    print_loop('c');
    char t1val = thread_join(t1);
    char t2val = thread_join(t2);
    printf("\nAll threads joined\n");
    printf("T1 returned: %c\n", t1val);
    printf("T2 returned: %c\n", t2val);
  } else {
    print_all_routes();
  }

  return 0;
}
