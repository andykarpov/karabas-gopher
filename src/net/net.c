#include <string.h>
#include "net.h"
#include "tr_ts.h"

net_t net;

void net_detect(void) BANKED
{
  memset(net.found, 0, sizeof(net.found));
  net.active = 0;
  net.core_zifi = ts_zifi_api();
  if (!net.core_zifi) return;          // the RS232 ports are part of the same API
  at_begin(&tr_zifi);
  net.found[BK_ZIFI] = at_probe();
  at_begin(&tr_rs232);
  net.found[BK_USB] = at_probe();
}

uint8_t net_use(uint8_t backend) BANKED
{
  if (backend >= BK_COUNT || !net.found[backend]) return 0;
  if (backend == BK_ZIFI) at_begin(&tr_zifi);
  else if (backend == BK_USB) at_begin(&tr_rs232);
  else return 0;                       // ZiFi32: Ф4
  net.active = backend;
  return 1;
}

uint8_t net_auto(void) BANKED
{
  uint8_t b;
  if (cfg.backend != BK_AUTO && net_use(cfg.backend)) return cfg.backend;
  for (b = 1; b < BK_COUNT; b++)
    if (net_use(b)) return b;
  return 0;
}

uint8_t net_status(void) BANKED { return at_status(&net.st); }
uint8_t net_scan(void) BANKED { return at_scan(net.aps, &net.naps); }
uint8_t net_join(const char *ssid, const char *pass) BANKED { return at_join(ssid, pass, &net.join_err); }
uint8_t net_forget(void) BANKED { return at_forget(); }
uint8_t net_open(const char *host, uint16_t port) BANKED { return at_tcp_open(host, port); }
uint8_t net_send(const uint8_t *buf, uint16_t n) BANKED { return at_tcp_send(buf, n); }
int16_t net_recv(uint8_t *buf, uint16_t max) BANKED { return at_tcp_recv(buf, max); }
void net_close(void) BANKED { at_tcp_close(); }
