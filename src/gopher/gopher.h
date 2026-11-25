// Fetching a gopher URL into the document (bank BANK_NET).
#pragma once
#include <stdint.h>
#include "../bank.h"
#include "url.h"

enum { G_OK, G_NONET, G_NOCONN, G_SEND, G_CANCEL, G_TIMEOUT, G_NOSEL };

// u in RAM; wrap 0 = menu rows, else text wrapped at that width; *bytes = received
uint8_t gopher_fetch(const gurl_t *u, uint8_t wrap) BANKED;
extern uint32_t gopher_bytes;
// the browser draws the progress (bank BANK_UI): called about every 2 KB
void browse_progress(void) BANKED;
// a line of the document (RAM out, max bytes): for the browser
uint16_t gopher_row(uint16_t i, char *out, uint16_t max) BANKED;
uint16_t gopher_rows(void) BANKED;
uint8_t gopher_full(void) BANKED;
// bookmarks / files read by bank 1 go into the document through this
void gopher_doc_reset(uint8_t cs, uint8_t wrap) BANKED;
void gopher_doc_append(const uint8_t *buf, uint16_t n) BANKED;
void gopher_doc_end(void) BANKED;
