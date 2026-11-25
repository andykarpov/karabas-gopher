// The network backends (docs/PLAN.md 3), bank BANK_NET. The data the screens read are in RAM.
#pragma once
#include <stdint.h>
#include "../bank.h"
#include "../config.h"
#include "at.h"

typedef struct {
  uint8_t core_zifi;        // the core has the ZiFi API
  uint8_t found[BK_COUNT];  // a modem answered on that backend
  uint8_t active;           // BK_NONE (0 = BK_AUTO used as none) or the backend in use
  at_status_t st;           // the last status
  kg_wifi_ap_t aps[AT_APS]; // the last scan
  uint8_t naps;
  int8_t join_err;
} net_t;

extern net_t net;

void net_detect(void) BANKED;                 // probes ZiFi and USB serial (ZiFi32: Ф4)
uint8_t net_use(uint8_t backend) BANKED;      // 0 if that backend has no modem
uint8_t net_auto(void) BANKED;                // cfg.backend or the first found; the backend or 0
uint8_t net_status(void) BANKED;              // -> net.st
uint8_t net_scan(void) BANKED;                // -> net.aps / net.naps
uint8_t net_join(const char *ssid, const char *pass) BANKED;   // RAM strings; -> net.join_err
uint8_t net_forget(void) BANKED;
uint8_t net_open(const char *host, uint16_t port) BANKED;      // RAM string
uint8_t net_send(const uint8_t *buf, uint16_t n) BANKED;
int16_t net_recv(uint8_t *buf, uint16_t max) BANKED;           // as at_tcp_recv
void net_close(void) BANKED;
