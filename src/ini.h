// INI text kept in a buffer: get / set keep the user's comments, order and line ends.
// [section] and key names are case-insensitive; "key = value", spaces around trimmed;
// lines starting with ';' or '#' are comments; no comments after a value (passwords may hold ';').
#pragma once
#include <stdint.h>

typedef struct {
  char *buf;          // the text, zero-terminated
  uint16_t len, cap;  // cap includes the terminating zero
} ini_t;

void ini_init(ini_t *ini, char *buf, uint16_t cap, uint16_t len);   // buf holds len bytes of text
// the value into out (cut to outmax - 1 characters); 0 if there is no such key
uint8_t ini_get(const ini_t *ini, const char *sect, const char *key, char *out, uint8_t outmax);
// 0 if the buffer is full (the text is left unchanged)
uint8_t ini_set(ini_t *ini, const char *sect, const char *key, const char *value);
