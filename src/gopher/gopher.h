// Fetching a gopher URL into the document (bank BANK_NET).
#pragma once
#include <stdint.h>
#include "../bank.h"
#include "url.h"

enum { G_OK, G_NONET, G_NOCONN, G_SEND, G_CANCEL, G_TIMEOUT, G_NOSEL, G_NOWIFI, G_FILE, G_WRITE, G_BIG };

// u in RAM; wrap 0 = menu rows, else text wrapped at that width; *bytes = received
uint8_t gopher_fetch(const gurl_t *u, uint8_t wrap) BANKED;
extern uint32_t gopher_bytes;
extern uint16_t gopher_t0;            // ticks at the start of the data
extern uint8_t gopher_saving;         // 1 while gopher_save runs, 2 gopher_load (for browse_progress)
extern uint8_t gopher_fr;             // the FatFs code of G_FILE / G_WRITE (DL_FULL: the card is full)
// a download into the file path ("dir/name" in RAM, room = its buffer; the name may get "_N");
// a text item (type 0) loses its "." line; a failed / cancelled download is deleted
uint8_t gopher_save(const gurl_t *u, char *path, uint8_t room) BANKED;
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
// a music module into the pages PG_MUSIC.. at PT3_MOD (the player must be stopped); G_BIG if it
// is longer than PT3_MAX; gopher_bytes = its length
uint8_t gopher_load(const gurl_t *u) BANKED;
