// The network backends (docs/PLAN.md 3), bank BANK_NET. The data the screens read are in RAM.
#pragma once
#include <stdint.h>
#include "../bank.h"
#include "../config.h"
#include "at.h"
#include "zifi32.h"
#include "../gopher/url.h"

typedef struct {
  uint8_t core_zifi;        // the core has the ZiFi API
  uint8_t found[BK_COUNT];  // a modem answered on that backend
  uint8_t active;           // BK_NONE (0 = BK_AUTO used as none) or the backend in use
  at_status_t st;           // the last status
  kg_wifi_ap_t aps[AT_APS]; // the last scan
  uint8_t naps;
  int8_t join_err;
  zf_info_t zf;             // ZiFi32 firmware
} net_t;

// the link for the header (net_link): state, network, signal, security
enum { LK_NONE, LK_DOWN, LK_JOINING, LK_UP };   // LK_NONE: no backend / no Wi-Fi (USB without one)
typedef struct {
  uint8_t state;
  char ssid[KG_WIFI_SSID_MAX + 1];
  int8_t rssi;              // 0 = unknown
  uint8_t ecn;              // 0 open, 0xFF unknown (from the last scan)
} net_link_t;

extern net_t net;
extern net_link_t net_lk;
extern uint8_t net_bg;     // netplat.c: 1 = a background poll, net_idle leaves the keys alone

void net_detect(void) BANKED;                 // probes ZiFi and USB serial (ZiFi32: Ф4)
uint8_t net_use(uint8_t backend) BANKED;      // 0 if that backend has no modem
uint8_t net_auto(void) BANKED;                // cfg.backend or the first found; the backend or 0
uint8_t net_status(void) BANKED;              // -> net.st
uint8_t net_scan(void) BANKED;                // -> net.aps / net.naps
uint8_t net_join(const char *ssid, const char *pass) BANKED;   // RAM strings; -> net.join_err
uint8_t net_forget(void) BANKED;
uint8_t net_open(const char *host, uint16_t port) BANKED;      // RAM string (AT backends)
uint8_t net_send(const uint8_t *buf, uint16_t n) BANKED;
// a gopher request (selector + CR LF) on any backend: AT_OK, AT_CANCEL, AT_ERROR; NET_NOSEL for a
// selector ZiFi32 cannot send
#define NET_NOSEL 0x10
uint8_t net_gopher_open(const gurl_t *u) BANKED;
uint8_t net_zf_reset(void) BANKED;
uint8_t net_zf_status(void) BANKED;          // the module STATUS of the last failed ZiFi32 command
int16_t net_recv(uint8_t *buf, uint16_t max) BANKED;           // as at_tcp_recv; -2 = the module failed
void net_close(void) BANKED;
// a short poll of the link (AT+CWJAP? / NETSTATE) -> net_lk; only while the network is free
void net_link(void) BANKED;
