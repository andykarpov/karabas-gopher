#include <string.h>
#ifdef HOST
#include "pic_host_ts.h"                 // tests/host/shim: pages in an array
#else
#include "ts.h"
#endif
#include "pic.h"

#define C(r, g, b) (uint16_t)(((uint16_t)(r) << 10) | ((g) << 5) | (b))

static uint8_t vconf[2];                // VConfig of each buffer
static uint16_t ncol[2];                // colours in its palette (16 / 256)
static uint8_t bb[360];                 // RAM bounce: a source row (windows 1, 2 hold this code)

static uint8_t lower(char c) { return (uint8_t)c | 0x20; }

uint8_t pic_format(const char *sel, uint8_t slen) BANKED
{
  const char *t = sel + slen;
  if (slen < 4) return PICF_NONE;
  if (t[-3] == '.' && lower(t[-2]) == 'm' && lower(t[-1]) == 'c') return PICF_MC;
  if (t[-4] != '.') return PICF_NONE;
  t -= 3;
  if (lower(t[0]) == 's' && lower(t[1]) == 'c' && lower(t[2]) == 'r') return PICF_SCR;
  if (lower(t[0]) == 'm' && lower(t[1]) == 'l' && lower(t[2]) == 't') return PICF_MC;
  if (lower(t[0]) == 'i' && lower(t[1]) == 'm' && lower(t[2]) == 'g') return PICF_IMG;
  if (lower(t[0]) == 'b' && lower(t[1]) == 'm' && lower(t[2]) == 'p') return PICF_BMP;
  return PICF_NONE;
}

// n bytes of the big buffer from off into bb (or out)
static void big_read(uint32_t off, uint8_t *out, uint16_t n)
{
  while (n) {
    uint16_t o = (uint16_t)off & 0x3FFF, k = 0x4000 - o;
    if (k > n) k = n;
    PAGE3(PG_BIG + (uint8_t)(off >> 14));
    memcpy(out, WIN3 + o, k);
    out += k; off += k; n -= k;
  }
}

// the CRAM word of a ZX colour (bit 0 blue, 1 red, 2 green, 3 bright); levels as the WC palette
static uint16_t zx_col(uint8_t c)
{
  uint8_t l = c & 8 ? 24 : 16;
  return C(c & 2 ? l : 0, c & 4 ? l : 0, c & 1 ? l : 0);
}

static void pal_set(uint8_t buf, uint16_t i, uint16_t w)
{
  PAGE3(PG_PICPAL);
  ((uint16_t *)(WIN3 + buf * 512))[i] = w;
}

// a ZX screen line y: its offset in the pixel area
static uint16_t zx_line(uint8_t y)
{
  return ((uint16_t)(y & 0xC0) << 5) | ((uint16_t)(y & 7) << 8) | ((uint16_t)(y & 0x38) << 2);
}

static uint8_t ink(uint8_t a) { return (a & 7) | ((a >> 3) & 8); }
static uint8_t paper(uint8_t a) { return ((a >> 3) & 7) | ((a >> 3) & 8); }

static uint8_t do_scr(uint32_t len, uint8_t pg, uint8_t buf)
{
  uint16_t o;
  uint8_t i;
  if (len < 6144) return PICE_SIZE;
  for (o = 0; o < 6912; o += 256) {                      // 27 pieces: 6144 is a boundary
    if (o < 6144 || len >= 6912) big_read(o, bb, 256);
    else memset(bb, 0x38, 256);                          // no attributes: black on white
    PAGE3(pg);
    memcpy(WIN3 + o, bb, 256);
  }
  for (i = 0; i < 16; i++) pal_set(buf, i, zx_col(i));
  vconf[buf] = 0x00;                                     // ZX, 256x192
  ncol[buf] = 16;
  return PICE_OK;
}

static uint8_t do_mc(uint32_t len, uint8_t pg, uint8_t buf)
{
  uint8_t y, x, b, a, i, n, f;
  uint8_t *p;
  if (len < 12288) return PICE_SIZE;
  for (y = 0; y < 192; y++) {
    big_read((uint16_t)y * 32, bb, 32);
    big_read(6144 + (uint16_t)y * 32, bb + 32, 32);
    PAGE3(pg + (y >> 6));
    p = WIN3 + ((uint16_t)(y & 63) << 8);                 // 16c: 256 bytes a line
    for (x = 0; x < 32; x++) {
      b = bb[x];
      a = bb[32 + x];
      n = ink(a);
      f = paper(a);
      for (i = 0; i < 4; i++) {
        *p++ = ((b & 0x80 ? n : f) << 4) | (b & 0x40 ? n : f);
        b <<= 2;
      }
    }
  }
  for (i = 0; i < 16; i++) pal_set(buf, i, zx_col(i));
  vconf[buf] = 0x01;                                     // 16c, 256x192
  ncol[buf] = 16;
  return PICE_OK;
}

static uint8_t do_img(uint32_t len, uint8_t pg, uint8_t buf)
{
  uint8_t y, x, b1, b2, a1, a2, i, j;
  uint8_t *p;
  uint16_t z;
  if (len < 13824) return PICE_SIZE;
  for (y = 0; y < 192; y++) {
    z = zx_line(y);
    big_read(z, bb, 32);
    big_read(6912 + z, bb + 32, 32);
    big_read(6144 + (uint16_t)(y >> 3) * 32, bb + 64, 32);
    big_read(6912 + 6144 + (uint16_t)(y >> 3) * 32, bb + 96, 32);
    PAGE3(pg + (y >> 5));
    p = WIN3 + ((uint16_t)(y & 31) << 9);                 // 256c: 512 bytes a line
    for (x = 0; x < 32; x++) {
      b1 = bb[x]; b2 = bb[32 + x]; a1 = bb[64 + x]; a2 = bb[96 + x];
      for (i = 0; i < 8; i++) {
        *p++ = ((b1 & 0x80 ? ink(a1) : paper(a1)) << 4) | (b2 & 0x80 ? ink(a2) : paper(a2));
        b1 <<= 1; b2 <<= 1;
      }
    }
  }
  // colour (c1 << 4 | c2) = the mean of the two ZX colours
  for (i = 0; i < 16; i++)
    for (j = 0; j < 16; j++) {
      uint16_t u = zx_col(i), v = zx_col(j);
      pal_set(buf, (i << 4) | j, C((((u >> 10) & 31) + ((v >> 10) & 31)) / 2,
                                   (((u >> 5) & 31) + ((v >> 5) & 31)) / 2, ((u & 31) + (v & 31)) / 2));
    }
  vconf[buf] = 0x02;                                     // 256c, 256x192
  ncol[buf] = 256;
  return PICE_OK;
}

static uint32_t le32(const uint8_t *p)
{
  return p[0] | (uint16_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

// screen sizes of RRES 0..3
static const uint16_t res_w[4] = { 256, 320, 320, 360 };
static const uint16_t res_h[4] = { 192, 200, 240, 288 };

static uint8_t do_bmp(uint32_t len, uint8_t pg, uint8_t buf)
{
  uint32_t data, hsize;
  uint16_t w, h, bpp, ncolors, stride, y, sh, sw, x0, y0, i, lb;
  uint8_t r, top_down = 0, bpl;
  if (len < 54) return PICE_SIZE;
  big_read(0, bb, 54);
  if (bb[0] != 'B' || bb[1] != 'M' || le32(bb + 30)) return PICE_BMP;   // BM, no compression
  data = le32(bb + 10);
  hsize = le32(bb + 14);
  w = (uint16_t)le32(bb + 18);
  if ((int32_t)le32(bb + 22) < 0) { top_down = 1; h = (uint16_t)-(int32_t)le32(bb + 22); }
  else h = (uint16_t)le32(bb + 22);
  bpp = bb[28];
  if ((bpp != 4 && bpp != 8) || !w || !h || w > 360 || h > 288) return PICE_BMP;
  ncolors = (uint16_t)le32(bb + 46);
  if (!ncolors || ncolors > (1u << bpp)) ncolors = 1u << bpp;
  for (r = 0; r < 3 && (w > res_w[r] || h > res_h[r]); r++) ;
  sw = res_w[r]; sh = res_h[r];
  x0 = (sw - w) / 2 & ~1u;                               // 16c: whole bytes
  y0 = (sh - h) / 2;
  stride = (uint16_t)(((uint32_t)w * bpp + 31) / 32 * 4);
  bpl = bpp == 4 ? 1 : 2;                                // a line: 256 or 512 bytes (in 256s)
  if (data + (uint32_t)stride * h > len) return PICE_SIZE;
  // the palette (B, G, R, 0), the rest black
  for (i = 0; i < (1u << bpp); i++) {
    uint16_t c = 0;
    if (i < ncolors) {
      big_read(14 + hsize + i * 4, bb, 4);
      c = C((bb[2] * 3 + 16) >> 5, (bb[1] * 3 + 16) >> 5, (bb[0] * 3 + 16) >> 5);
    }
    pal_set(buf, i, c);
  }
  // the screen lines: colour 0 around the picture
  for (y = 0; y < sh; y++) {
    uint32_t a = (uint32_t)y << (7 + bpl);               // 256 or 512 bytes a line
    PAGE3(pg + (uint8_t)(a >> 14));
    memset(WIN3 + ((uint16_t)a & 0x3FFF), 0, (uint16_t)1 << (7 + bpl));
  }
  lb = bpp == 4 ? (w + 1) / 2 : w;                       // bytes of a picture line
  for (y = 0; y < h; y++) {
    uint32_t a = (uint32_t)(y0 + y) << (7 + bpl);
    big_read(data + (uint32_t)stride * (top_down ? y : h - 1 - y), bb, lb);
    a += bpp == 4 ? x0 / 2 : x0;
    PAGE3(pg + (uint8_t)(a >> 14));
    memcpy(WIN3 + ((uint16_t)a & 0x3FFF), bb, lb);
  }
  vconf[buf] = (r << 6) | (bpp == 4 ? 0x01 : 0x02);
  ncol[buf] = 1u << bpp;
  return PICE_OK;
}

uint8_t pic_decode(uint8_t fmt, uint32_t len, uint8_t buf) BANKED
{
  uint8_t pg = buf ? PG_PICB : PG_PICA;
  switch (fmt) {
    case PICF_SCR: return do_scr(len, pg, buf);
    case PICF_MC: return do_mc(len, pg, buf);
    case PICF_IMG: return do_img(len, pg, buf);
    case PICF_BMP: return do_bmp(len, pg, buf);
  }
  return PICE_BMP;
}

void pic_show(uint8_t buf) BANKED
{
  uint16_t w[16], i;
  for (i = 0; i < ncol[buf]; i += 16) {
    PAGE3(PG_PICPAL);
    memcpy(w, WIN3 + buf * 512 + i * 2, 32);
    __critical {
      TS_FMADDR = FM_EN | (0xC000 >> 12);
      memcpy(WIN3 + i * 2, w, 32);
      TS_FMADDR = 0;
    }
  }
#ifndef HOST
  __asm__("halt");                                       // the switch at a frame start
#endif
  TS_VPAGE = buf ? PG_PICB : PG_PICA;
  TS_PALSEL = 0;
  TS_BORDER = 0;
  TS_GXOFFSL = 0; TS_GXOFFSH = 0;
  TS_GYOFFSL = 0; TS_GYOFFSH = 0;
  TS_VCONFIG = vconf[buf];
}
