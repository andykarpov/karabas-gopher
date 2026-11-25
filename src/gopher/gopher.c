#include <string.h>
#include "ts.h"
#include "gopher.h"
#include "doc.h"
#include "../dl.h"
#include "../net/net.h"


uint32_t gopher_bytes;
uint16_t gopher_t0;
uint8_t gopher_saving, gopher_fr;
static uint8_t buf[128];

uint8_t *doc_map(uint8_t page)
{
  TS_PAGE3 = page >= DOC_IDX ? PG_IDX + (page - DOC_IDX) : PG_DOC + page;
  return WIN3;
}

// to_file 0: into the document, 1: a file, 2: a text file (its "." line is cut)
static uint8_t receive(const gurl_t *u, uint8_t to_file)
{
  uint8_t r;
  int16_t n;
  uint16_t t0, shown = 0;
  uint8_t tail[5];
  // net_recv: > 0 data, 0 nothing yet, -1 the end, -2 the module failed
  gopher_bytes = 0;
  gopher_fr = 0;
  memset(tail, 0, sizeof(tail));
  if (!net.active) return G_NONET;
  if (net.active == BK_ZIFI32 && zf_netstate() != 2) return G_NOWIFI;
  r = net_gopher_open(u);                // the selector (may hold TAB + words) + CR LF
  if (r == AT_CANCEL) return G_CANCEL;
  if (r == NET_NOSEL) return G_NOSEL;
  if (r) return G_NOCONN;
  gopher_t0 = t0 = ticks;
  while ((n = net_recv(buf, sizeof(buf))) != -1) {
    if (n == -2) { r = G_TIMEOUT; goto out; }
    if (n < 0) { r = G_CANCEL; goto out; }      // -3: Esc seen by the ZiFi32 driver
    if (!n) {
      if ((uint16_t)(ticks - t0) > 50 * 20) { r = G_TIMEOUT; goto out; }
      if (net_idle()) { r = G_CANCEL; goto out; }
      continue;
    }
    t0 = ticks;
    gopher_bytes += (uint16_t)n;
    if (!to_file) doc_append(buf, (uint16_t)n);   // a full document drops the rest, the link is read out
    else {
      if ((gopher_fr = dl_write(buf, (uint16_t)n))) { r = G_WRITE; goto out; }
      if (n >= 5) memcpy(tail, buf + n - 5, 5);
      else { memmove(tail, tail + n, 5 - n); memcpy(tail + 5 - n, buf, n); }
    }
    if ((uint16_t)(gopher_bytes >> 10) != shown) { shown = (uint16_t)(gopher_bytes >> 10); browse_progress(); }
  }
  r = G_OK;
out:
  net_close();
  if (!to_file) doc_end();
  else if (r == G_OK) {
    // a text file ends with "." on its own line: not part of the file
    uint8_t cut = to_file == 2 && !memcmp(tail + 2, ".\r\n", 3) &&
                  (gopher_bytes == 3 || (tail[0] == '\r' && tail[1] == '\n')) ? 3 : 0;
    if ((gopher_fr = dl_close(1, cut))) r = G_WRITE;
  }
  return r;
}

uint8_t gopher_fetch(const gurl_t *u, uint8_t wrap) BANKED
{
  doc_reset(cfg.charset, wrap);
  gopher_saving = 0;
  return receive(u, 0);
}

uint8_t gopher_save(const gurl_t *u, char *path, uint8_t room) BANKED
{
  uint8_t r;
  gopher_bytes = 0;
  gopher_saving = 1;
  if ((gopher_fr = dl_open(path, room))) return G_FILE;
  r = receive(u, u->type == '0' ? 2 : 1);
  if (r != G_OK) dl_close(0, 0);         // deleted (after a failed dl_close it is already closed)
  return r;
}

uint16_t gopher_row(uint16_t i, char *out, uint16_t max) BANKED { return doc_row(i, out, max); }
uint16_t gopher_rows(void) BANKED { return doc_rows(); }
uint8_t gopher_full(void) BANKED { return doc_full(); }
void gopher_doc_reset(uint8_t cs, uint8_t wrap) BANKED { doc_reset(cs, wrap); }
void gopher_doc_append(const uint8_t *b, uint16_t n) BANKED { doc_append(b, n); }
void gopher_doc_end(void) BANKED { doc_end(); }
