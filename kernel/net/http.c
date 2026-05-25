#include "http.h"
#include "io/serial.h"
#include "net/socket.h"

void http_test() {
  // build request
  // char req[] = "GET / HTTP/1.0\r\nHost: 10.0.2.2\r\n\r\n";
  // socket_handle s = socket_tcp_client(IP(10, 0, 2, 2), 8000, 0);
  // socket_send(s, req, sizeof(req) - 1);
  //
  // char buf[4096];
  // int n = socket_recv(s, buf, sizeof(buf));
  // serial_printf("HTTP TEST RECEIVE LENGTH: %d", n);
  //// buf now has "HTTP/1.0 200 OK\r\n...headers...\r\n\r\nbody"
  // socket_close(s);
}