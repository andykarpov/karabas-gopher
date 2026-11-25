// One gopher menu line: "Tdisplay<TAB>selector<TAB>host<TAB>port". Plain C (host tests).
#pragma once
#include <stdint.h>

typedef struct {
  char type;
  const char *disp; uint8_t dlen;     // spans in the line (not zero-terminated)
  const char *sel;  uint8_t slen;
  const char *host; uint8_t hlen;
  uint16_t port;
} gitem_t;

// line without CR LF; 0 for the end line "." (and empty lines)
uint8_t menu_item(const char *line, uint16_t len, gitem_t *it);
// 1 for types the browser opens as a link (menu, text, search, binaries, images, html)
uint8_t menu_link(char type);
