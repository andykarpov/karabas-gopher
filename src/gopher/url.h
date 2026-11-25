// gopher://host[:port]/[type][selector] (RFC 4266; %XX in the selector). Plain C (host tests).
#pragma once
#include <stdint.h>

#define URL_HOST 64
#define URL_SEL  256

typedef struct {
  char host[URL_HOST];
  uint16_t port;
  char type;               // the item type, '1' (menu) if the URL has none
  char sel[URL_SEL];       // decoded selector (may hold a TAB + search words)
} gurl_t;

// "gopher://host:70/1/sel", "host/0/x.txt", "host" (the scheme may be left out); 0 if no host
uint8_t url_parse(const char *s, gurl_t *u);
// back to text: "gopher://host[:port]/Tsel", '%', TAB, CR, LF and spaces escaped; cut to max - 1
void url_format(const gurl_t *u, char *out, uint16_t max);
