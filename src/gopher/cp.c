#include "cp.h"
#include "cp_tables.h"

static uint8_t uni(uint16_t u)
{
  uint8_t lo = 0, hi = sizeof(uni_cp) / sizeof(uni_cp[0]);
  while (lo < hi) {
    uint8_t m = (lo + hi) / 2;
    if (uni_cp[m] == u) return uni_866[m];
    if (uni_cp[m] < u) lo = m + 1; else hi = m;
  }
  return '?';
}

uint8_t cp_conv(uint8_t cs, const char *in, uint16_t n, char *out, uint8_t max)
{
  const uint8_t *p = (const uint8_t *)in, *e = p + n;
  uint8_t k = 0, c;
  while (p < e && k < max) {
    c = *p++;
    if (c == '\t') { do out[k++] = ' '; while ((k & 7) && k < max); continue; }
    if (c < 0x80) { out[k++] = (char)(c < 0x20 ? ' ' : c); continue; }
    if (cs == 1) { out[k++] = (char)cp1251_866[c - 0x80]; continue; }
    if (cs == 2) { out[k++] = (char)koi8_866[c - 0x80]; continue; }
    if (cs == 3) { out[k++] = (char)c; continue; }
    // UTF-8
    if (c < 0xC0) continue;                    // a stray continuation byte
    {
      uint16_t u;
      uint8_t more = c < 0xE0 ? 1 : c < 0xF0 ? 2 : 3;
      if (e - p < more) break;
      if (more == 1) u = (uint16_t)((c & 0x1F) << 6 | (p[0] & 0x3F));
      else if (more == 2) u = (uint16_t)((c & 0x0F) << 12 | (p[0] & 0x3F) << 6 | (p[1] & 0x3F));
      else u = 0xFFFF;                         // outside the BMP: no CP866 for it
      p += more;
      out[k++] = (char)uni(u);
    }
  }
  out[k] = 0;
  return k;
}
