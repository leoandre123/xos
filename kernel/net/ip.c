#include "ip.h"
#include "io/logging.h"
#include "memory/heap.h"
#include "memory/memutils.h"
#include "net/net.h"
#include "net/networking.h"
#include "net/routing.h"
#include "net/socket.h"
#include "net_types.h"
#include "types.h"

typedef struct {
  ipv4_addr ip;
  void *packet;
  ushort packet_len;
  int exists;
} pending_ip_packet;

// ipv4_addr g_ip;

static ushort
ip_checksum(void *data, ushort len) {
  ushort *ptr = (ushort *)data;
  uint sum = 0;
  while (len > 1) {
    sum += *ptr++;
    len -= 2;
  }
  if (len)
    sum += *(ubyte *)ptr;
  while (sum >> 16)
    sum = (sum & 0xFFFF) + (sum >> 16);
  return ~sum;
}

void ip_send(ipv4_addr dst_addr, ubyte protocol, void *payload, ushort payload_len, ip_send_opts opts) {
  static ushort ip_id = 0;
  nic *nic = 0;
  route best_route = {0};
  if (opts.nic_id) {
    for (int i = 0; i < MAX_NICS; i++) {
      if (g_nics[i].nic_id == opts.nic_id) {
        nic = &g_nics[i];
        klogf(LOG_TRACE, "SUPEROFUND, %x", nic);
        find_best_route_for_nic(nic->nic_id, dst_addr, &best_route);
        break;
      }
    }
  } else if (opts.src_addr.value == 0) {
    find_best_route(dst_addr, &best_route);
    nic = &g_nics[best_route.nic_id];
  } else {
    for (int i = 0; i < MAX_NICS; i++) {
      if (g_nics[i].addr.value == opts.src_addr.value) {
        nic = &g_nics[i];
        find_best_route_for_nic(nic->nic_id, dst_addr, &best_route);
        break;
      }
    }
  }

  if (!nic || !best_route.nic_id) {
    klogf(LOG_WARNING, "No NIC found: nic_id: %d, local addr: %d.%d.%d.%d",
          opts.nic_id,
          opts.src_addr.parts[0],
          opts.src_addr.parts[1],
          opts.src_addr.parts[2],
          opts.src_addr.parts[3]);

    klogf(LOG_TRACE, "Available NICs");
    for (int i = 0; i < MAX_NICS; i++) {
      if (g_nics[i].nic_id) {
        klogf(LOG_TRACE, "Nic-%d (%d)", i, g_nics[i].nic_id);
        if (g_nics[i].nic_id == opts.nic_id) {
          klogf(LOG_TRACE, "FOUND");
        }
      }
    }
    print_all_routes();
    return;
  }

  ipv4_header header;
  header.version_ihl = (4 << 4) | 5;
  header.dscp_ecn = 0;
  header.total_length = htons(sizeof(ipv4_header) + payload_len);
  header.identification = htons(ip_id++);
  header.flags_fragment_offset = htons(1 << 14);
  header.ttl = 64;
  header.protocol = protocol;
  header.header_checksum = 0;
  header.src_addr = nic->addr;
  header.dst_addr = dst_addr;

  header.header_checksum = ip_checksum(&header, sizeof(ipv4_header));

  ushort total = sizeof(ipv4_header) + payload_len;
  void *packet = kmalloc(total);
  memcpy8(packet, (ubyte *)&header, sizeof(ipv4_header));
  memcpy8(((ubyte *)packet) + sizeof(ipv4_header), payload, payload_len);

  ipv4_addr next_hop = best_route.gateway.value ? best_route.gateway : dst_addr;
  nic->driver->send(nic, packet, total, next_hop);
  kfree(packet);
}

void ip_receive(void *data, ushort data_len) {
  if (data_len < sizeof(ipv4_header)) {
    return;
  }
  socket_on_data(data, data_len);
}