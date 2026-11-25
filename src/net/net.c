#include <string.h>
#include "net.h"
#include "tr_ts.h"
#include "zifi32.h"

net_t net;

void net_detect(void) BANKED
{
  memset(net.found, 0, sizeof(net.found));
  net.active = 0;
  net.found[BK_ZIFI32] = zf_detect(&net.zf);
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
  else if (backend == BK_ZIFI32) {
    // the network of gopher.ini (the module keeps none of its own over SPI)
    if (cfg.autoconn && cfg.ssid[0] && zf_netstate() != 2) zf_join(cfg.ssid, cfg.pass);
  } else return 0;
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

static uint8_t zf_err(uint8_t r)
{
  return r == ZF_OK ? AT_OK : r == ZF_CANCEL ? AT_CANCEL : r == ZF_TIMEOUT ? AT_TIMEOUT : AT_ERROR;
}

uint8_t net_status(void) BANKED
{
  if (net.active != BK_ZIFI32) return at_status(&net.st);
  memset(&net.st, 0, sizeof(net.st));
  net.st.mode = net.st.flow = -1;
  {
    char *v = net.st.version;
    uint8_t i;
    for (i = 0; i < 3; i++) {
      uint8_t x = net.zf.ver[i];
      if (x >= 10) *v++ = '0' + x / 10;
      *v++ = '0' + x % 10;
      *v++ = i < 2 ? '.' : 0;
    }
  }
  if (zf_netstate() == 2) {
    strcpy(net.st.ssid, cfg.ssid[0] ? cfg.ssid : "?");
    zf_own_ip(net.st.ip);
  }
  return AT_OK;
}

uint8_t net_scan(void) BANKED
{
  if (net.active == BK_ZIFI32) return zf_err(zf_scan(net.aps, &net.naps, AT_APS));
  return at_scan(net.aps, &net.naps);
}

uint8_t net_join(const char *ssid, const char *pass) BANKED
{
  uint8_t r;
  if (net.active != BK_ZIFI32) return at_join(ssid, pass, &net.join_err);
  net.join_err = -1;
  r = zf_join(ssid, pass);
  if (r == ZF_OK) {
    // kept in gopher.ini: the module forgets it on a restart
    strcpy(cfg.ssid, ssid);
    strcpy(cfg.pass, pass);
    cfg_save();
  }
  return zf_err(r);
}

uint8_t net_forget(void) BANKED
{
  if (net.active != BK_ZIFI32) return at_forget();
  cfg.ssid[0] = cfg.pass[0] = 0;
  cfg_save();
  return zf_err(zf_leave());
}

uint8_t net_zf_reset(void) BANKED { return zf_err(zf_reset()); }

uint8_t net_open(const char *host, uint16_t port) BANKED { return at_tcp_open(host, port); }
uint8_t net_send(const uint8_t *buf, uint16_t n) BANKED { return at_tcp_send(buf, n); }

uint8_t net_gopher_open(const gurl_t *u) BANKED
{
  uint8_t r;
  if (net.active == BK_ZIFI32) {
    r = zf_gopher_open(u->host, u->port, u->sel);
    return r == ZF_NOSEL ? NET_NOSEL : zf_err(r);
  }
  if ((r = at_tcp_open(u->host, u->port))) return r;
  {
    uint16_t len = (uint16_t)strlen(u->sel);
    if ((len && (r = at_tcp_send((const uint8_t *)u->sel, len))) || (r = at_tcp_send((const uint8_t *)"\r\n", 2))) {
      at_tcp_close();
      return r;
    }
  }
  return AT_OK;
}

int16_t net_recv(uint8_t *buf, uint16_t max) BANKED
{
  return net.active == BK_ZIFI32 ? zf_gopher_read(buf, max) : at_tcp_recv(buf, max);
}

void net_close(void) BANKED
{
  if (net.active == BK_ZIFI32) zf_gopher_close();
  else at_tcp_close();
}
