#include <string.h>
#include <stdlib.h>
#include "at.h"

static const transport_t *tr;
static char line[AT_LINE_MAX];
static uint8_t len;
static uint16_t ipd_left;              // raw TCP bytes still to come in the current +IPD
static uint8_t ring[AT_RING];
static uint16_t rd, wr;                // ring positions (free running)
static uint8_t closed, prompt, send_ok, result;   // result: 0xFF while a command runs
static at_line_fn line_fn;
static char cmd_buf[2 * KG_WIFI_SSID_MAX + 2 * 64 + 24];

#define RING_USED() ((uint16_t)(wr - rd))
#define CHUNK 64

void at_begin(const transport_t *t)
{
  tr = t;
  len = 0;
  ipd_left = 0;
  rd = wr = 0;
  closed = 1;
  if (tr->clear) tr->clear();
}

static uint8_t starts(const char *s, const char *p)
{
  return !strncmp(s, p, strlen(p));
}

static void end_line(void)
{
  line[len] = 0;
  if (!len) return;
  len = 0;
  if (!strcmp(line, "OK")) { result = AT_OK; return; }
  if (!strcmp(line, "SEND OK")) { send_ok = 1; return; }
  if (!strcmp(line, "ERROR") || !strcmp(line, "FAIL") || !strcmp(line, "SEND FAIL")) { result = AT_ERROR; return; }
  if (!strcmp(line, "CLOSED") || strstr(line, ",CLOSED")) { closed = 1; return; }
  if (!strcmp(line, "CONNECT") || strstr(line, ",CONNECT")) { closed = 0; return; }
  if (starts(line, "busy ") || starts(line, "Recv ") || starts(line, "WIFI ")) return;
  if (line_fn) line_fn(line);
}

static void byte_in(uint8_t c)
{
  if (ipd_left) {
    ring[wr++ & (AT_RING - 1)] = c;
    ipd_left--;
    return;
  }
  if (c == '\n') { end_line(); return; }
  if (c == '\r') return;
  if (len < AT_LINE_MAX - 1) line[len++] = (char)c;
  if (c == ':' && len > 5 && !strncmp(line, "+IPD,", 5)) {
    // +IPD,n: or +IPD,id,n: -> n raw bytes follow
    char *p = strrchr(line, ',');
    line[len - 1] = 0;
    ipd_left = (uint16_t)atoi(p + 1);
    len = 0;
    return;
  }
  if (len == 1 && c == '>') { prompt = 1; len = 0; }
}

// takes what the transport has, as long as the ring has room for a whole chunk
static void pump(void)
{
  uint8_t buf[CHUNK], n, i;
  for (;;) {
    if (AT_RING - RING_USED() < CHUNK) return;
    n = tr->avail();
    if (!n) return;
    if (n > CHUNK) n = CHUNK;
    tr->read(buf, n);
    for (i = 0; i < n; i++) byte_in(buf[i]);
  }
}

static void send(const void *p, uint16_t n)
{
  const uint8_t *b = (const uint8_t *)p;
  uint8_t r;
  while (n) {
    r = tr->room();
    if (!r) { pump(); continue; }
    if (r > n) r = (uint8_t)n;
    tr->write(b, r);
    b += r;
    n -= r;
  }
}

// waits until cond() or a command result; AT_TIMEOUT / AT_CANCEL otherwise
static uint8_t wait(uint8_t (*cond)(void), uint16_t timeout)
{
  uint16_t t0 = net_ticks();
  for (;;) {
    pump();
    if (cond && cond()) return AT_OK;
    if (result != 0xFF) return result;
    if ((uint16_t)(net_ticks() - t0) > timeout) return AT_TIMEOUT;
    if (net_idle()) return AT_CANCEL;
  }
}

uint8_t at_cmd(const char *cmd, uint16_t timeout, at_line_fn fn)
{
  uint8_t r;
  pump();
  line_fn = fn;
  result = 0xFF;
  send(cmd, (uint16_t)strlen(cmd));
  send("\r\n", 2);
  r = wait(0, timeout);
  line_fn = 0;
  return r;
}

uint8_t at_probe(void)
{
  uint8_t i;
  for (i = 0; i < 2; i++) {
    at_cmd("ATE0", 25, 0);
    if (at_cmd("AT", 25, 0) == AT_OK) return 1;
  }
  return 0;
}

// ---- status
static at_status_t *st_out;
static void st_line(const char *l)
{
  int v;
  kg_esp_parse_version(l, st_out->version, sizeof(st_out->version));
  if ((v = kg_esp_parse_mode(l)) >= 0) st_out->mode = (int8_t)v;
  if ((v = kg_esp_parse_flow(l)) >= 0) st_out->flow = (int8_t)v;
  kg_esp_parse_cwjap(l, st_out->ssid, sizeof(st_out->ssid));
  kg_esp_parse_ip(l, st_out->ip, sizeof(st_out->ip));
}

uint8_t at_status(at_status_t *st)
{
  uint8_t r;
  memset(st, 0, sizeof(*st));
  st->mode = st->flow = -1;
  st_out = st;
  if ((r = at_cmd("AT+GMR", 100, st_line))) return r;
  at_cmd("AT+CWMODE?", 50, st_line);
  at_cmd("AT+UART_DEF?", 50, st_line);
  at_cmd("AT+CWJAP?", 100, st_line);
  if (st->ssid[0]) at_cmd("AT+CIFSR", 100, st_line);
  if (!strcmp(st->ip, "0.0.0.0")) st->ip[0] = 0;
  return AT_OK;
}

// ---- scan / join / forget
static kg_wifi_ap_t *aps_out;
static uint8_t *aps_n;
static void scan_line(const char *l)
{
  kg_wifi_ap_t ap;
  if (kg_esp_parse_cwlap(l, &ap)) kg_wifi_ap_add(aps_out, aps_n, AT_APS, &ap);
}

uint8_t at_scan(kg_wifi_ap_t *aps, uint8_t *count)
{
  aps_out = aps;
  aps_n = count;
  *count = 0;
  return at_cmd("AT+CWLAP", 50 * 15, scan_line);
}

static int8_t join_err;
static void join_line(const char *l)
{
  int e = kg_esp_join_error(l);
  if (e >= 0) join_err = (int8_t)e;
}

uint8_t at_join(const char *ssid, const char *pass, int8_t *err)
{
  uint8_t r;
  char *p = cmd_buf;
  strcpy(p, "AT+CWJAP=\"");
  kg_esp_quote(ssid, p + strlen(p), 2 * KG_WIFI_SSID_MAX + 1);
  strcat(p, "\",\"");
  kg_esp_quote(pass, p + strlen(p), 2 * 64 + 1);
  strcat(p, "\"");
  join_err = -1;
  r = at_cmd(cmd_buf, 50 * 25, join_line);
  *err = join_err;
  if (r == AT_OK) at_cmd("AT+CWAUTOCONN=1", 50, 0);
  return r;
}

uint8_t at_forget(void)
{
  at_cmd("AT+CWAUTOCONN=0", 50, 0);
  return at_cmd("AT+CWQAP", 100, 0);
}

// ---- TCP
static uint8_t got_prompt(void) { return prompt; }
static uint8_t got_send_ok(void) { return send_ok; }

static char *utoa16(uint16_t v, char *p)
{
  char b[6];
  uint8_t i = 5;
  b[5] = 0;
  do { b[--i] = '0' + v % 10; v /= 10; } while (v);
  strcpy(p, b + i);
  return p + strlen(p);
}

uint8_t at_tcp_open(const char *host, uint16_t port)
{
  uint8_t r;
  if (strlen(host) > 100) return AT_ERROR;
  at_cmd("AT+CIPCLOSE", 50, 0);            // a link left over: ALREADY CONNECTED otherwise
  at_cmd("AT+CIPMUX=0", 50, 0);
  rd = wr = 0;
  ipd_left = 0;
  strcpy(cmd_buf, "AT+CIPSTART=\"TCP\",\"");
  strcat(cmd_buf, host);
  strcat(cmd_buf, "\",");
  utoa16(port, cmd_buf + strlen(cmd_buf));
  closed = 1;
  r = at_cmd(cmd_buf, 50 * 20, 0);
  if (r == AT_OK) closed = 0;
  return r;
}

uint8_t at_tcp_send(const uint8_t *buf, uint16_t n)
{
  uint8_t r;
  strcpy(cmd_buf, "AT+CIPSEND=");
  utoa16(n, cmd_buf + strlen(cmd_buf));
  prompt = send_ok = 0;
  result = 0xFF;
  send(cmd_buf, (uint16_t)strlen(cmd_buf));
  send("\r\n", 2);
  r = wait(got_prompt, 50 * 5);            // "OK" first, then "> "
  if (r == AT_OK && !prompt) {
    result = 0xFF;
    r = wait(got_prompt, 50 * 5);
  }
  if (!prompt) return r == AT_OK ? AT_ERROR : r;
  result = 0xFF;
  send(buf, n);
  r = wait(got_send_ok, 50 * 10);
  return send_ok ? AT_OK : (r == AT_OK ? AT_ERROR : r);
}

int16_t at_tcp_recv(uint8_t *buf, uint16_t max)
{
  uint16_t n = 0;
  pump();
  while (n < max && RING_USED()) {
    buf[n++] = ring[rd++ & (AT_RING - 1)];
  }
  if (n) pump();
  if (!n && closed && !ipd_left) return -1;
  return (int16_t)n;
}

void at_tcp_close(void)
{
  if (!closed) at_cmd("AT+CIPCLOSE", 50, 0);
  closed = 1;
}
