#include "dhcp.h"
#include "io/e1000.h"
#include "io/serial.h"
#include "net/net.h"
#include "net/udp.h"

void dhcp_send_discovery() {
  dhcp_packet packet = {0};

  packet.op = 1;
  packet.htype = 1;
  packet.hlen = 6;
  packet.xid = htonl(0x1337);
  e1000_get_mac(packet.client_hw_addr);
  packet.magic_cookie = htonl(0x63825363);
  packet.flags = htons(0x8000); // broadcast flag

  // DHCP options
  packet.options[0] = 53; // option 53: DHCP message type
  packet.options[1] = 1;  // length 1
  packet.options[2] = 1;  // value: Discover

  packet.options[3] = 50; // requested IP
  packet.options[4] = 4;  // length 4
  packet.options[5] = 10;
  packet.options[6] = 0;
  packet.options[7] = 2;
  packet.options[8] = 69;

  packet.options[9] = 255; // end option

  ipv4_addr broadcast_addr = IP(255, 255, 255, 255);
  udp_send(broadcast_addr, 68, 67, &packet, sizeof(dhcp_packet));
}

void dhcp_receive(void *data, ushort data_len) {
  serial_write_line("DHCP: received packet");
  if (data_len < sizeof(dhcp_packet)) {
    return;
  }

  dhcp_packet *header = data;

  serial_write("DHCP:");
  serial_write("Your IP: ");
  serial_printf("%d:%d:%d:%d", header->your_addr[0], header->your_addr[1], header->your_addr[2], header->your_addr[3]);
}