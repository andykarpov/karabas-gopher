#include <string.h>
#include "url.h"

static int8_t hex(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

uint8_t url_parse(const char *s, gurl_t *u)
{
  uint8_t n = 0;
  uint16_t k = 0;
  memset(u, 0, sizeof(*u));
  u->port = 70;
  u->type = '1';
  while (*s == ' ') s++;
  if (!strncmp(s, "gopher://", 9)) s += 9;
  while (*s && *s != '/' && *s != ':' && n < URL_HOST - 1) u->host[n++] = *s++;
  u->host[n] = 0;
  if (!n) return 0;
  if (*s == ':') {
    uint16_t p = 0;
    s++;
    while (*s >= '0' && *s <= '9') p = p * 10 + (*s++ - '0');
    if (p) u->port = p;
  }
  if (*s != '/') return 1;
  s++;
  if (!*s) return 1;
  u->type = *s++;
  while (*s && k < URL_SEL - 1) {
    if (*s == '%' && hex(s[1]) >= 0 && hex(s[2]) >= 0) {
      u->sel[k++] = (char)(hex(s[1]) << 4 | hex(s[2]));
      s += 3;
    } else {
      u->sel[k++] = *s++;
    }
  }
  u->sel[k] = 0;
  return 1;
}

static void put(char *out, uint16_t max, uint16_t *k, char c)
{
  if (*k + 1 < max) out[(*k)++] = c;
}

void url_format(const gurl_t *u, char *out, uint16_t max)
{
  static const char hx[] = "0123456789ABCDEF";
  uint16_t k = 0;
  const char *p;
  for (p = "gopher://"; *p; p++) put(out, max, &k, *p);
  for (p = u->host; *p; p++) put(out, max, &k, *p);
  if (u->port != 70) {
    char b[6];
    uint8_t i = 5;
    uint16_t v = u->port;
    b[5] = 0;
    do { b[--i] = '0' + v % 10; v /= 10; } while (v);
    put(out, max, &k, ':');
    for (p = b + i; *p; p++) put(out, max, &k, *p);
  }
  put(out, max, &k, '/');
  if (u->type != '1' || u->sel[0]) put(out, max, &k, u->type);
  for (p = u->sel; *p; p++) {
    if (*p == '%' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ' ') {
      if (k + 3 >= max) break;
      put(out, max, &k, '%');
      put(out, max, &k, hx[(uint8_t)*p >> 4]);
      put(out, max, &k, hx[*p & 15]);
    } else {
      put(out, max, &k, *p);
    }
  }
  out[k] = 0;
}

void url_filename(const char *sel, char *out, uint8_t max)
{
  const char *b = sel, *e, *p;
  uint8_t n, x;
  for (e = sel; *e && *e != '\t'; e++) ;
  while (e > sel && e[-1] == '/') e--;                 // "dir/" -> "dir"
  for (p = sel; p < e; p++) if (*p == '/' || *p == '\\') b = p + 1;
  while (b < e && (*b == ' ' || *b == '.')) b++;
  while (e > b && (e[-1] == ' ' || e[-1] == '.')) e--;
  if (b == e) { b = "file"; e = b + 4; }
  n = (uint8_t)(e - b > 250 ? 250 : e - b);
  if (n > max - 1) {
    // keep the extension: "very-long-name.scr" -> "very-lo.scr"
    for (p = e - 1; p > b && p > e - 6 && *p != '.'; p--) ;
    x = *p == '.' && p > b ? (uint8_t)(e - p) : 0;
    memcpy(out, b, max - 1 - x);
    memcpy(out + max - 1 - x, e - x, x);
    n = max - 1;
  } else memcpy(out, b, n);
  out[n] = 0;
  for (x = 0; x < n; x++) {
    uint8_t c = (uint8_t)out[x];
    if (c < 0x20 || c == 0x7F || strchr("\"*:<>?|", c)) out[x] = '_';
  }
}
