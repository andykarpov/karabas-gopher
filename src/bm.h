// Bookmarks: /zifi/bookmarks.txt, lines "URL<TAB>name" (name in CP866), bank BANK_FS.
#pragma once
#include <stdint.h>
#include "bank.h"
#define BM_PATH "/zifi/bookmarks.txt"
// into the document (gopher_doc_*), a row a bookmark; FatFs result (FR_NO_FILE = none yet)
uint8_t bm_load(void) BANKED;
uint8_t bm_add(const char *line) BANKED;          // RAM line without CR LF
uint8_t bm_delete(uint16_t row) BANKED;           // rewrites the file from the loaded document
