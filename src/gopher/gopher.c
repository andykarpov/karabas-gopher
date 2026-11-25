#include <string.h>
#include "ts.h"
#include "gopher.h"
#include "doc.h"
#include "../net/net.h"

#define PG_DOC  0x40       // 16 data pages #40..#4F
#define PG_IDX  0x3E       // index pages #3E, #3F

uint32_t gopher_bytes;
static uint8_t buf[128];

uint8_t *doc_map(uint8_t page)
{
  TS_PAGE3 = page >= DOC_IDX ? PG_IDX + (page - DOC_IDX) : PG_DOC + page;
  return WIN3;
}

uint8_t gopher_fetch(const gurl_t *u, uint8_t wrap) BANKED
{
  uint8_t r;
  int16_t n;
  uint16_t t0, shown = 0;
  gopher_bytes = 0;
  doc_reset(cfg.charset, wrap);
  if (!net.active) return G_NONET;
  r = net_open(u->host, u->port);
  if (r == AT_CANCEL) return G_CANCEL;
  if (r) return G_NOCONN;
  {
    // the request: selector (may hold TAB + words) CR LF, sent in pieces through buf
    uint16_t len = (uint16_t)strlen(u->sel), off = 0;
    while (off < len) {
      uint16_t k = len - off > sizeof(buf) ? sizeof(buf) : len - off;
      memcpy(buf, u->sel + off, k);
      if (net_send(buf, k)) { net_close(); return G_SEND; }
      off += k;
    }
    buf[0] = '\r'; buf[1] = '\n';
    if (net_send(buf, 2)) { net_close(); return G_SEND; }
  }
  t0 = ticks;
  while ((n = net_recv(buf, sizeof(buf))) >= 0) {
    if (!n) {
      if ((uint16_t)(ticks - t0) > 50 * 20) { net_close(); doc_end(); return G_TIMEOUT; }
      if (net_idle()) { net_close(); doc_end(); return G_CANCEL; }
      continue;
    }
    t0 = ticks;
    gopher_bytes += (uint16_t)n;
    doc_append(buf, (uint16_t)n);       // a full document drops the rest, the link is read out
    if ((uint16_t)(gopher_bytes >> 11) != shown) { shown = (uint16_t)(gopher_bytes >> 11); browse_progress(); }
  }
  net_close();
  doc_end();
  return G_OK;
}

uint16_t gopher_row(uint16_t i, char *out, uint16_t max) BANKED { return doc_row(i, out, max); }
uint16_t gopher_rows(void) BANKED { return doc_rows(); }
uint8_t gopher_full(void) BANKED { return doc_full(); }
void gopher_doc_reset(uint8_t cs, uint8_t wrap) BANKED { doc_reset(cs, wrap); }
void gopher_doc_append(const uint8_t *b, uint16_t n) BANKED { doc_append(b, n); }
void gopher_doc_end(void) BANKED { doc_end(); }
