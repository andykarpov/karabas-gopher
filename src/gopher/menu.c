#include <string.h>
#include "menu.h"

static uint8_t field(const char **p, const char *e, const char **out)
{
  const char *s = *p;
  while (*p < e && **p != '\t') (*p)++;
  *out = s;
  {
    uint16_t n = (uint16_t)(*p - s);
    if (*p < e) (*p)++;                  // the TAB
    return n > 255 ? 255 : (uint8_t)n;
  }
}

uint8_t menu_item(const char *line, uint16_t len, gitem_t *it)
{
  const char *p = line + 1, *e = line + len, *port;
  uint8_t n;
  memset(it, 0, sizeof(*it));
  if (!len || (len == 1 && line[0] == '.')) return 0;
  it->type = line[0];
  it->dlen = field(&p, e, &it->disp);
  it->slen = field(&p, e, &it->sel);
  it->hlen = field(&p, e, &it->host);
  n = field(&p, e, &port);
  it->port = 0;
  while (n-- && *port >= '0' && *port <= '9') it->port = it->port * 10 + (*port++ - '0');
  if (!it->port) it->port = 70;
  return 1;
}

uint8_t menu_link(char t)
{
  return t != 'i' && t != '3' && t != '.' ;
}
