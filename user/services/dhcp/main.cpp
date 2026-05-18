#include "memory.h"
#include "net_types.h"
#include "socket.h"
#include "syscall.h"
#include "syscalls.h"
#include <stdio.h>

typedef struct {
  ubyte op;
  ubyte htype;
  ubyte hlen;
  ubyte hops;
  uint xid;
  ushort secs;
  ushort flags;
  ipv4_addr client_addr;
  ipv4_addr your_addr;
  ipv4_addr server_addr;
  ipv4_addr gateway_addr;
  ubyte client_hw_addr[16];
  ubyte overflow[192];
  uint magic_cookie;
  ubyte options[64];
} __attribute__((__packed__)) dhcp_packet;

static inline uint htonl(uint x) { return __builtin_bswap32(x); }
static inline ushort htons(ushort x) { return __builtin_bswap16(x); }
#define ntohl htonl
#define ntohs htons

static inline void sys_net_get_mac(ubyte mac[6]) {
  syscall(SYS_NET_GET_MAC, (ulong)mac, 0, 0);
}
static inline void sys_net_set_ip(ipv4_addr ip) {
  syscall(SYS_NET_SET_IP, ip.value, 0, 0);
}

static ubyte parse_msg_type(const dhcp_packet *pkt) {
  for (int i = 0; i < 62;) {
    if (pkt->options[i] == 255)
      break;
    if (pkt->options[i] == 53)
      return pkt->options[i + 2];
    i += 2 + pkt->options[i + 1];
  }
  return 0;
}

int main() {
  sys_write("DHCP: starting\n");

  ubyte mac[6] = {0};
  sys_net_get_mac(mac);

  ipv4_addr broadcast = {.value = 0xFFFFFFFF};
  socket sock = socket_udp_open(broadcast, 67, 68);
  if (!sock) {
    sys_write("DHCP: failed to open socket\n");
    return 1;
  }

  // --- DHCPDISCOVER ---
  dhcp_packet pkt = {0};
  pkt.op = 1;
  pkt.htype = 1;
  pkt.hlen = 6;
  pkt.xid = htonl(0x3713);
  pkt.flags = htons(0x8000);
  pkt.magic_cookie = htonl(0x63825363);
  memcpy(pkt.client_hw_addr, mac, 6);
  int opt = 0;
  pkt.options[opt++] = 53;
  pkt.options[opt++] = 1;
  pkt.options[opt++] = 1; // DISCOVER
  pkt.options[opt++] = 255;

  socket_udp_send(sock, &pkt, sizeof(dhcp_packet));
  sys_write("DHCP: sent discover\n");

  // --- wait for DHCPOFFER ---
  dhcp_packet offer = {0};
  socket_udp_recv(sock, &offer, sizeof(dhcp_packet));
  if (parse_msg_type(&offer) != 2) {
    sys_write("DHCP: expected OFFER\n");
    socket_close(sock);
    return 1;
  }
  sys_write("DHCP: got offer\n");

  ipv4_addr offered_ip = offer.your_addr;
  ipv4_addr server_ip = offer.server_addr;

  // --- DHCPREQUEST ---
  dhcp_packet req = {0};
  req.op = 1;
  req.htype = 1;
  req.hlen = 6;
  req.xid = htonl(0x3713);
  req.flags = htons(0x8000);
  req.magic_cookie = htonl(0x63825363);
  memcpy(req.client_hw_addr, mac, 6);
  opt = 0;
  req.options[opt++] = 53;
  req.options[opt++] = 1;
  req.options[opt++] = 3; // REQUEST
  req.options[opt++] = 50;
  req.options[opt++] = 4; // requested IP
  req.options[opt++] = offered_ip.parts[0];
  req.options[opt++] = offered_ip.parts[1];
  req.options[opt++] = offered_ip.parts[2];
  req.options[opt++] = offered_ip.parts[3];
  req.options[opt++] = 54;
  req.options[opt++] = 4; // server ID
  req.options[opt++] = server_ip.parts[0];
  req.options[opt++] = server_ip.parts[1];
  req.options[opt++] = server_ip.parts[2];
  req.options[opt++] = server_ip.parts[3];
  req.options[opt++] = 255;

  socket_udp_send(sock, &req, sizeof(dhcp_packet));
  sys_write("DHCP: sent request\n");

  // --- wait for DHCPACK ---
  dhcp_packet ack = {0};
  socket_udp_recv(sock, &ack, sizeof(dhcp_packet));
  if (parse_msg_type(&ack) != 5) {
    sys_write("DHCP: expected ACK\n");
    socket_close(sock);
    return 1;
  }

  ipv4_addr assigned = ack.your_addr;
  sys_net_set_ip(assigned);

  char buf[64];
  sprintf(buf, "DHCP: assigned %d.%d.%d.%d\n", assigned.parts[0],
          assigned.parts[1], assigned.parts[2], assigned.parts[3]);
  sys_write(buf);

  socket_close(sock);
  return 0;
}
