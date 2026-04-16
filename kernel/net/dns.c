#include "dns.h"
#include "memory/heap.h"
#include "memory/memutils.h"
#include "net/net.h"
#include "net/socket.h"
#include "net/udp.h"
#include "net_types.h"
#include "types.h"

static ipv4_addr g_dns_server_addr = (ipv4_addr)0x0302000Au;

void dns_resolve(const char *host) {
  dns_header header = {0};
  header.id = htons(0x1337);
  header.flags = htons(0x0100); // RD = recursion desired
  header.qdcount = htons(1);

  ubyte qname_buf[256];
  int qnl = 0;
  int qni = 1;
  // www.google.com
  //
  while (*host) {
    if (*host == '.') {
      qname_buf[qnl] = qni - qnl - 1;
      qnl = qni++;
    } else {
      qname_buf[qni++] = *host;
    }
    host++;
  }
  qname_buf[qnl] = qni - qnl - 1; // length of last label
  qname_buf[qni++] = 0;

  ushort total_size = sizeof(header) + qni + 4;

  void *payload = kmalloc(total_size);
  int offset = 0;
  memcpy8(payload, (ubyte *)&header, sizeof(dns_header));
  offset += sizeof(dns_header);
  memcpy8(((ubyte *)payload) + offset, qname_buf, qni);
  offset += qni;
  memset16((ushort *)(((ubyte *)payload) + offset), htons(1), 1);
  offset += 2;
  memset16((ushort *)(((ubyte *)payload) + offset), htons(1), 1);
  offset += 2;

  socket_handle handle = socket_udp(g_dns_server_addr, 53, 1024);
  socket_send(handle, payload, total_size);
  socket_close(handle);
  kfree(payload);
}
void dns_set_server_ip(ipv4_addr ip) {
  g_dns_server_addr = ip;
}