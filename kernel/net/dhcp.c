#include "dhcp.h"
#include "io/e1000.h"
#include "io/serial.h"
#include "net/net.h"
#include "net_types.h"
#include "udp.h"

ipv4_addr g_ip = {.value = 0};

void dhcp_send_discovery() {
  dhcp_packet packet = {0};

  packet.op = 1;
  packet.htype = 1;
  packet.hlen = 6;
  packet.xid = htonl(0x1337);
  e1000_get_mac((mac_addr *)&packet.client_hw_addr);
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

  ipv4_addr broadcast_addr = {.value = 0xFFFFFFFF};
  udp_send_old(broadcast_addr, 68, 67, &packet, sizeof(dhcp_packet));
}

static void dhcp_send_request(ipv4_addr offered_ip, ipv4_addr server_ip) {
  dhcp_packet packet = {0};

  packet.op = 1;
  packet.htype = 1;
  packet.hlen = 6;
  packet.xid = htonl(0x1337);
  e1000_get_mac((mac_addr *)&packet.client_hw_addr);
  packet.magic_cookie = htonl(0x63825363);
  packet.flags = htons(0x8000);

  packet.options[0] = 53; // DHCP message type
  packet.options[1] = 1;
  packet.options[2] = 3; // Request

  packet.options[3] = 50; // requested IP
  packet.options[4] = 4;
  packet.options[5] = offered_ip.parts[0];
  packet.options[6] = offered_ip.parts[1];
  packet.options[7] = offered_ip.parts[2];
  packet.options[8] = offered_ip.parts[3];

  packet.options[9] = 54; // server identifier
  packet.options[10] = 4;
  packet.options[11] = server_ip.parts[0];
  packet.options[12] = server_ip.parts[1];
  packet.options[13] = server_ip.parts[2];
  packet.options[14] = server_ip.parts[3];

  packet.options[15] = 255; // end

  ipv4_addr broadcast_addr = {.value = 0xFFFFFFFF};
  udp_send_old(broadcast_addr, 68, 67, &packet, sizeof(dhcp_packet));
}

void dhcp_receive(void *data, ushort data_len) {
  serial_write_line("DHCP: received packet");
  if (data_len < sizeof(dhcp_packet)) {
    return;
  }

  dhcp_packet *header = data;

  // check message type option (byte 53)
  ubyte msg_type = 0;
  for (int i = 0; i < 64 - 2;) {
    if (header->options[i] == 255)
      break;
    if (header->options[i] == 53) {
      msg_type = header->options[i + 2];
      break;
    }
    i += 2 + header->options[i + 1];
  }

  if (msg_type == 2) { // OFFER
    serial_write_line("DHCP: got offer, sending request");
    dhcp_send_request(header->your_addr, header->server_addr);
  } else if (msg_type == 5) { // ACK
    g_ip = header->your_addr;
    serial_printf("DHCP ACK - IP: %d.%d.%d.%d\n",
                  g_ip.parts[0], g_ip.parts[1], g_ip.parts[2], g_ip.parts[3]);
  }
}