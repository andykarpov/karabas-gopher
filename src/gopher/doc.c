#include <string.h>
#include "doc.h"
#include "cp.h"

static uint8_t cs, wrap, full, pending, col;
static uint8_t end_pg;          // the end of the data: page, offset
static uint16_t end_off;
static uint16_t rows;

static void add_row(uint8_t pg, uint16_t off)
{
  uint8_t *p;
  if (rows >= DOC_ROWS) return;
  p = doc_map(DOC_IDX + (rows >> 12)) + ((rows & 0x0FFF) << 2);
  p[0] = (uint8_t)off; p[1] = (uint8_t)(off >> 8); p[2] = pg;
  rows++;
}

static void row_at(uint16_t i, uint8_t *pg, uint16_t *off)
{
  uint8_t *p = doc_map(DOC_IDX + (i >> 12)) + ((i & 0x0FFF) << 2);
  *off = p[0] | (uint16_t)p[1] << 8;
  *pg = p[2];
}

void doc_reset(uint8_t charset, uint8_t width)
{
  cs = charset;
  wrap = width;
  full = 0;
  pending = 1;
  col = 0;
  end_pg = 0;
  end_off = 0;
  rows = 0;
}

uint8_t doc_append(const uint8_t *buf, uint16_t n)
{
  uint8_t *page;
  uint8_t b;
  if (full) return 0;
  page = doc_map(end_pg);
  while (n--) {
    b = *buf++;
    if (pending) { add_row(end_pg, end_off); pending = 0; col = 0; page = doc_map(end_pg); }
    if (b == '\n') pending = 1;
    else if (wrap && b != '\r' && CP_STARTS(cs, b)) {
      if (col >= wrap) { add_row(end_pg, end_off); col = 0; page = doc_map(end_pg); }
      col = b == '\t' ? (uint8_t)((col + 8) & ~7) : col + 1;
    }
    page[end_off] = b;
    if (++end_off == 0x4000) {
      end_off = 0;
      if (++end_pg == DOC_PAGES) { full = 1; end_pg--; end_off = 0x4000; return 0; }
      page = doc_map(end_pg);
    }
  }
  return 1;
}

uint16_t doc_row(uint16_t i, char *out, uint16_t max)
{
  uint8_t pg, epg;
  uint16_t off, eoff, k = 0;
  uint8_t *page;
  if (i >= rows) return 0;
  row_at(i, &pg, &off);
  if (i + 1 < rows) row_at(i + 1, &epg, &eoff);
  else { epg = end_pg; eoff = end_off; }
  page = doc_map(pg);
  while ((pg < epg || (pg == epg && off < eoff)) && k < max) {
    char c = (char)page[off];
    if (c == '\n') break;
    if (c != '\r') out[k++] = c;
    if (++off == 0x4000) { off = 0; if (++pg >= DOC_PAGES) break; page = doc_map(pg); }
  }
  return k;
}

void doc_end(void)
{
  char c[2];
  if (rows && doc_row(rows - 1, c, 2) == 1 && c[0] == '.') rows--;
}

uint16_t doc_rows(void) { return rows; }
uint8_t doc_full(void) { return full || rows >= DOC_ROWS; }
