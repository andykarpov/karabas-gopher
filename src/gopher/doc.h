// The document being shown: raw bytes in pages of 16 KB + an index of its screen rows (a menu: one
// row a line; text: lines wrapped at a width, TABs to 8 columns, as cp_conv draws them).
// Plain C: the pages come from doc_map() (Z80: window 3; host tests: arrays).
#pragma once
#include <stdint.h>

#define DOC_PAGES    16          // 256 KB of data
#define DOC_IDX_PG   2           // 2 index pages: 4 bytes a row
#define DOC_ROWS     (DOC_IDX_PG * 4096)

// page 0..DOC_PAGES-1: data, DOC_IDX + n: index page n; returns the 16 KB of it
#define DOC_IDX      0x80
uint8_t *doc_map(uint8_t page);

void doc_reset(uint8_t cs, uint8_t wrap);              // wrap 0: a row a line (menus)
uint8_t doc_append(const uint8_t *buf, uint16_t n);    // 0 when full (the rest is dropped)
void doc_end(void);                                    // drops a last row "."
uint16_t doc_rows(void);
uint8_t doc_full(void);
// the raw bytes of row i, without CR / LF, at most max; returns their count
uint16_t doc_row(uint16_t i, char *out, uint16_t max);
