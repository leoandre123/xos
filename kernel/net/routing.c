#include "routing.h"
#include "io/logging.h"
#include "net_types.h"

static route s_routing_table[ROUTING_TABLE_MAX_ENTREIS] = {0};
static bool s_is_route_used[ROUTING_TABLE_MAX_ENTREIS] = {0};

void find_best_route(ipv4_addr dst, route *route_out) {
  route *best = 0;
  uint best_mask = 0;

  for (int i = 0; i < ROUTING_TABLE_MAX_ENTREIS; i++) {
    if (s_is_route_used[i]) {
      route *r = &s_routing_table[i];
      if ((dst.value & r->netmask.value) == (r->destination.value & r->netmask.value)) {
        if (r->netmask.value > best_mask) {
          best_mask = r->netmask.value;
          best = r;
        } else if (r->netmask.value == best_mask && r->metric < best->metric) {
          best_mask = r->netmask.value;
          best = r;
        }
      }
    }
  }

  if (best)
    *route_out = *best;
}

void find_best_route_for_nic(ulong nic_id, ipv4_addr dst, route *route_out) {
  route *best = 0;
  uint best_mask = 0;

  for (int i = 0; i < ROUTING_TABLE_MAX_ENTREIS; i++) {
    if (s_is_route_used[i]) {
      route *r = &s_routing_table[i];
      if (r->nic_id != nic_id)
        continue;
      if ((dst.value & r->netmask.value) == (r->destination.value & r->netmask.value)) {
        if (r->netmask.value > best_mask) {
          best_mask = r->netmask.value;
          best = r;
        } else if (r->netmask.value == best_mask && r->metric < best->metric) {
          best_mask = r->netmask.value;
          best = r;
        }
      }
    }
  }

  if (best)
    *route_out = *best;
}

void route_upsert(route rt) {
  klogf(LOG_TRACE, "Adding route");
  for (int i = 0; i < ROUTING_TABLE_MAX_ENTREIS; i++) {
    if (s_is_route_used[i]) {
      route *r = &s_routing_table[i];
      if (r->destination.value == rt.destination.value &&
          r->netmask.value == rt.netmask.value &&
          r->nic_id == rt.nic_id) {
        *r = rt;
        return;
      }
    }
  }
  for (int i = 0; i < ROUTING_TABLE_MAX_ENTREIS; i++) {
    if (!s_is_route_used[i]) {
      s_routing_table[i] = rt;
      s_is_route_used[i] = true;
      return;
    }
  }
}

void print_all_routes() {
  klogf(LOG_DEBUG, "|  Destination  |    Netmask    |    Gateway    |");
  for (int i = 0; i < ROUTING_TABLE_MAX_ENTREIS; i++) {
    if (s_is_route_used[i]) {
      route *r = &s_routing_table[i];
      klogf(LOG_DEBUG, " %3d.%3d.%3d.%3d %3d.%3d.%3d.%3d %3d.%3d.%3d.%3d",
            IPV4_SPILL(r->destination),
            IPV4_SPILL(r->netmask),
            IPV4_SPILL(r->gateway));
    }
  }
}

int get_routes(route_info *buf, int len) {
  int c = 0;
  for (int i = 0; i < ROUTING_TABLE_MAX_ENTREIS; i++) {
    if (s_is_route_used[i]) {
      route_info *info = &buf[c++];
      info->destination = s_routing_table[i].destination;
      info->netmask = s_routing_table[i].netmask;
      info->gateway = s_routing_table[i].gateway;
      info->nic_id = s_routing_table[i].nic_id;
      info->metric = s_routing_table[i].metric;
      if (c >= len) {
        break;
      }
    }
  }
  return c;
}