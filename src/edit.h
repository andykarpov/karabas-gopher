// One-line text editor (URL, query, settings). Portable C: the logic here, the drawing in ui.c.
#pragma once
#include <stdint.h>

typedef struct {
  char *buf;          // zero-terminated, max + 1 bytes
  uint8_t max;        // characters
  uint8_t len, pos;   // length, cursor
  uint8_t scroll;     // first shown character
  uint8_t width;      // shown characters (set by the caller)
  uint8_t ovr;        // 1: overwrite instead of insert
} edit_t;

enum { EDIT_GOING, EDIT_DONE, EDIT_CANCEL };

void edit_start(edit_t *e, char *buf, uint8_t max, uint8_t width);
// a key in (kbd.h); EDIT_DONE on Enter, EDIT_CANCEL on Esc; keeps scroll so that pos is shown
uint8_t edit_key(edit_t *e, uint16_t key);
