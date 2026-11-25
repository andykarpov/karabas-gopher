// ESP8266 AT driver (AT 1.x / 2.x, one TCP link, active receive mode) over a transport_t.
// Never sends AT+RST, AT+RESTORE, AT+UART_DEF or AT+SYSSTORE: the boot core set the module up.
// Plain C: the same file runs on the Z80 (bank 3) and in the host tests against tools/at_proxy.py.
#pragma once
#include <stdint.h>
#include "transport.h"
#include "esp_at_parse.h"

enum { AT_OK, AT_ERROR, AT_TIMEOUT, AT_CANCEL, AT_CLOSED };

#define AT_LINE_MAX  128
#define AT_RING      1024            // received TCP data not yet taken (a power of 2)
#define AT_APS       16

typedef void (*at_line_fn)(const char *line);

// the receive ring, AT_RING bytes (Z80: page PG_NETBUF in window 3, tr_ts.c; host: an array);
// also the chunk / URL buffer of zifi32.c
uint8_t *at_ring_map(void);

void at_begin(const transport_t *t);
// sends cmd + CR LF, waits for OK / ERROR / FAIL; other lines go to fn (may be 0); timeout in ticks
uint8_t at_cmd(const char *cmd, uint16_t timeout, at_line_fn fn);
// echo off, then AT -> OK: 1 if a modem answers
uint8_t at_probe(void);

typedef struct {
  char version[16];      // "1.7.4.0", empty if unknown
  int8_t mode;           // CWMODE: 1 station, 2 AP, 3 both, -1 unknown
  int8_t flow;           // UART_DEF flow control (2 = CTS), -1 unknown
  char ssid[KG_WIFI_SSID_MAX + 1];   // joined network, empty if none
  char ip[16];
} at_status_t;

uint8_t at_status(at_status_t *st);
// the networks around, strongest first, at most AT_APS
uint8_t at_scan(kg_wifi_ap_t *aps, uint8_t *count);
// joins and keeps the network in the module (AT+CWJAP + AT+CWAUTOCONN=1); *err = the +CWJAP code
uint8_t at_join(const char *ssid, const char *pass, int8_t *err);
// AT+CWAUTOCONN=0 + AT+CWQAP
uint8_t at_forget(void);

uint8_t at_tcp_open(const char *host, uint16_t port);
uint8_t at_tcp_send(const uint8_t *buf, uint16_t n);
// > 0: bytes, 0: none yet, -1: the link closed and all its data taken
int16_t at_tcp_recv(uint8_t *buf, uint16_t max);
void at_tcp_close(void);
