// Text of the servers -> CP866 (the screen font). Plain C (host tests).
#pragma once
#include <stdint.h>

// charsets: the order of config.h CS_* (utf8, cp1251, koi8, cp866)
// converts n bytes of in to at most max CP866 characters in out (zero-terminated: out holds max + 1);
// a UTF-8 sequence cut at the end of in is dropped; returns the characters written
uint8_t cp_conv(uint8_t cs, const char *in, uint16_t n, char *out, uint8_t max);
// 1 if byte c starts a character (UTF-8 continuation bytes do not), for counting columns
#define CP_STARTS(cs, c) ((cs) != 0 || ((uint8_t)(c) & 0xC0) != 0x80)
