#include "net_types.h"
#include "nic_info.h"
#include "socket.h"
#include "syscall.h"
#include "syscalls.h"
#include "thread.h"
#include "threads.h"
#include "time.h"
#include "utils.h"
#include <memory.h>
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
  ubyte options[244];
} __attribute__((__packed__)) dhcp_packet;

static inline uint htonl(uint x) { return __builtin_bswap32(x); }
static inline ushort htons(ushort x) { return __builtin_bswap16(x); }
#define ntohl htonl
#define ntohs htons

static inline int sys_net_nics(nic_info *infos, int count) {
  return syscall(SYS_NET_NICS, (ulong)infos, count, 0);
}
static inline void sys_net_get_mac(int nic, ubyte mac[6]) {
  syscall(SYS_NET_GET_MAC, nic, (ulong)mac, 0);
}
static inline void sys_net_set_ip(int nic, ipv4_addr ip) {
  syscall(SYS_NET_SET_IP, nic, ip.value, 0);
}
static inline void sys_net_conf_nic(int nic_id, nic_config_field field,
                                    ulong value) {
  syscall(SYS_NET_CONF_NIC, nic_id, field, value);
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

static inline int receive_with_timeout(socket_handle handle, void *buf,
                                       ushort len, ulong timeout) {
  int max_tries = timeout / 100;
  for (int i = 0; i < max_tries; i++) {
    int read = socket_recv_nb(handle, buf, len);
    if (read)
      return read;
    sleep(100);
  }
  return 0;
}

static void run(nic_info *nic) {
  char buf[128];
  sys_write("DHCP: starting\n");

  ubyte mac[6] = {0};
  sys_net_get_mac(nic->nic_id, mac);

  ipv4_addr broadcast = {.value = 0xFFFFFFFF};
  socket_addr remote_addr = {.protocol = SOCKET_UDP,
                             .udp_addr = {.addr = broadcast, .port = 67}};
  socket_addr local_addr = {.protocol = SOCKET_UDP,
                            .udp_addr = {.addr = {0}, .port = 68}};

  socket_handle sock = socket(SOCKET_UDP);

  if (!sock) {
    sys_write("DHCP: failed to open socket\n");
    return;
  }

  socket_bind(sock, &local_addr);
  socket_bind_nic(sock, nic->nic_id);
  socket_connect(sock, &remote_addr);

  sprintf(
      buf, "Socket created:\nlocal: %d.%d.%d.%d:%d\nremote: %d.%d.%d.%d:%d\n",
      local_addr.udp_addr.addr.parts[0], local_addr.udp_addr.addr.parts[1],
      local_addr.udp_addr.addr.parts[2], local_addr.udp_addr.addr.parts[3],
      local_addr.udp_addr.port, remote_addr.udp_addr.addr.parts[0],
      remote_addr.udp_addr.addr.parts[1], remote_addr.udp_addr.addr.parts[2],
      remote_addr.udp_addr.addr.parts[3], remote_addr.udp_addr.port);
  sys_write(buf);

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

  socket_send(sock, &pkt, sizeof(dhcp_packet));
  sys_write("DHCP: sent discover\n");

  // --- wait for DHCPOFFER ---
  dhcp_packet offer = {0};

  if (!receive_with_timeout(sock, &offer, sizeof(dhcp_packet), 5000)) {
    sys_write("DHCP: timeout\n");
    socket_close(sock);
    return;
  }
  if (parse_msg_type(&offer) != 2) {
    sys_write("DHCP: expected OFFER\n");
    socket_close(sock);
    return;
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

  socket_send(sock, &req, sizeof(dhcp_packet));
  sys_write("DHCP: sent request\n");

  // --- wait for DHCPACK ---
  dhcp_packet ack = {0};
  if (!receive_with_timeout(sock, &ack, sizeof(dhcp_packet), 5000)) {
    sys_write("DHCP: timeout\n");
    socket_close(sock);
    return;
  }
  if (parse_msg_type(&ack) != 5) {
    sys_write("DHCP: expected ACK\n");
    socket_close(sock);
    return;
  }

  int opt_ind = 0;
  ipv4_addr netmask;
  ipv4_addr gateway;
  while (opt_ind < 244) {
    int opt_type = ack.options[opt_ind++];
    int opt_len = ack.options[opt_ind++];

    if (opt_type == 1 && opt_len >= 4) {
      netmask.parts[0] = ack.options[opt_ind + 0];
      netmask.parts[1] = ack.options[opt_ind + 1];
      netmask.parts[2] = ack.options[opt_ind + 2];
      netmask.parts[3] = ack.options[opt_ind + 3];
    } else if (opt_type == 3 && opt_len >= 4) {
      gateway.parts[0] = ack.options[opt_ind + 0];
      gateway.parts[1] = ack.options[opt_ind + 1];
      gateway.parts[2] = ack.options[opt_ind + 2];
      gateway.parts[3] = ack.options[opt_ind + 3];
    }

    opt_ind += opt_len;
  }

  ipv4_addr assigned = ack.your_addr;
  sys_net_conf_nic(nic->nic_id, NIC_ADDRESS, (ulong)assigned.value);
  if (netmask.value)
    sys_net_conf_nic(nic->nic_id, NIC_NETMASK, (ulong)netmask.value);
  if (gateway.value)
    sys_net_conf_nic(nic->nic_id, NIC_GATEWAY, (ulong)gateway.value);

  sprintf(buf, "DHCP: assigned %d.%d.%d.%d\n", IPV4_SPILL(assigned));
  sys_write(buf);

  socket_close(sock);
  return;
}

int main() {
  nic_info infos[10];
  thread_handle threads[10];
  int count = sys_net_nics(infos, 10);
  char buf[30];
  sprintf(buf, "DHCP: Found %x nics\n", count);
  sys_write(buf);

  for (int i = 0; i < count; i++) {
    sprintf(buf, "NIC-%d (%d)\n", i, infos[i].nic_id);
    sys_write(buf);
  }
  for (int i = 0; i < count; i++) {
    threads[i] = thread_spawn((void *)run, (ulong)&infos[i]);
  }
  for (int i = 0; i < count; i++) {
    thread_join(threads[i]);
  }
  sys_write("DHCP DONE!!!!!!\n");
  return 0;
}