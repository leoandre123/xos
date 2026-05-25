#pragma once
#include "net_types.h"
#include "route_info.h"

#define ROUTING_TABLE_MAX_ENTREIS 256

typedef struct {
  ipv4_addr destination;
  ipv4_addr netmask;
  ipv4_addr gateway;
  int nic_id;
  int metric;
} route;

void find_best_route(ipv4_addr addr, route *route_out);
void find_best_route_for_nic(ulong nic_id, ipv4_addr dst, route *route_out);
void route_upsert(route route);
void print_all_routes();
int get_routes(route_info *buf, int len);