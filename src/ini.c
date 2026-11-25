#include <string.h>
#include "ini.h"

static char lower(char c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
static uint8_t is_sp(char c) { return c == ' ' || c == '\t'; }

// case-insensitive compare of [a, a + n) with the zero-terminated b
static uint8_t eq(const char *a, uint16_t n, const char *b)
{
  while (n--) {
    if (!*b || lower(*a) != lower(*b)) return 0;
    a++; b++;
  }
  return !*b;
}

void ini_init(ini_t *ini, char *buf, uint16_t cap, uint16_t len)
{
  ini->buf = buf;
  ini->cap = cap;
  ini->len = len < cap ? len : cap - 1;
  buf[ini->len] = 0;
}

// Finds the key. Returns 1 and the value span, or 0 with *ins = where a new key line would go
// (after the last line of the section, or 0xFFFF if there is no such section).
static uint8_t find(const ini_t *ini, const char *sect, const char *key,
                    uint16_t *vs, uint16_t *ve, uint16_t *ins)
{
  const char *b = ini->buf;
  uint16_t i = 0, ls, le, a, z;
  uint8_t in = 0;
  *ins = 0xFFFF;
  while (i < ini->len) {
    ls = i;
    while (i < ini->len && b[i] != '\n') i++;
    le = i;                               // line [ls, le)
    if (i < ini->len) i++;                // past '\n'
    a = ls;
    while (a < le && is_sp(b[a])) a++;
    z = le;
    while (z > a && (is_sp(b[z - 1]) || b[z - 1] == '\r')) z--;
    if (a == z || b[a] == ';' || b[a] == '#') continue;
    if (b[a] == '[') {
      const char *e = memchr(b + a, ']', z - a);
      if (in) return 0;                   // the section ended without the key
      in = e && eq(b + a + 1, (uint16_t)(e - b - a - 1), sect);
      if (in) *ins = i;
      continue;
    }
    if (!in) continue;
    *ins = i;
    {
      const char *eqs = memchr(b + a, '=', z - a);
      uint16_t k, ke;
      if (!eqs) continue;
      k = (uint16_t)(eqs - b);
      ke = k;
      while (ke > a && is_sp(b[ke - 1])) ke--;
      if (!eq(b + a, ke - a, key)) continue;
      k++;
      while (k < z && is_sp(b[k])) k++;
      *vs = k;
      *ve = z;
      return 1;
    }
  }
  return 0;
}

uint8_t ini_get(const ini_t *ini, const char *sect, const char *key, char *out, uint8_t outmax)
{
  uint16_t vs, ve, ins, n;
  if (!find(ini, sect, key, &vs, &ve, &ins)) return 0;
  n = ve - vs;
  if (n > outmax - 1) n = outmax - 1;
  memcpy(out, ini->buf + vs, n);
  out[n] = 0;
  return 1;
}

// replaces [at, at + del) with ins (n bytes)
static uint8_t splice(ini_t *ini, uint16_t at, uint16_t del, const char *ins, uint16_t n)
{
  if ((uint32_t)ini->len - del + n + 1 > ini->cap) return 0;
  memmove(ini->buf + at + n, ini->buf + at + del, ini->len - at - del + 1);
  memcpy(ini->buf + at, ins, n);
  ini->len = ini->len - del + n;
  return 1;
}

uint8_t ini_set(ini_t *ini, const char *sect, const char *key, const char *value)
{
  uint16_t vs, ve, ins;
  char line[300];             // a URL value is up to 255 characters
  const char *eol = memchr(ini->buf, '\r', ini->len) ? "\r\n" : "\n";
  uint16_t n;

  if (find(ini, sect, key, &vs, &ve, &ins))
    return splice(ini, vs, ve - vs, value, (uint16_t)strlen(value));

  if (strlen(sect) + strlen(key) + strlen(value) + 10 > sizeof(line)) return 0;
  line[0] = 0;
  if (ins == 0xFFFF) {                    // a new section at the end
    ins = ini->len;
    if (ins && ini->buf[ins - 1] != '\n') strcat(line, eol);
    if (ins) strcat(line, eol);
    strcat(line, "[");
    strcat(line, sect);
    strcat(line, "]");
    strcat(line, eol);
  } else if (ins && ini->buf[ins - 1] != '\n') {
    strcat(line, eol);                    // the last line had no line end
  }
  strcat(line, key);
  strcat(line, "=");
  strcat(line, value);
  strcat(line, eol);
  n = (uint16_t)strlen(line);
  return splice(ini, ins, 0, line, n);
}
