#pragma once
#include "net/net.h"
#include "types.h"

typedef struct {
  ushort id;
  ushort flags;
  ushort qdcount;
  ushort ancount;
  ushort nscount;
  ushort arcount;
} dns_header;

void dns_resolve(const char *host);
void dns_set_server_ip(ipv4_addr ip);