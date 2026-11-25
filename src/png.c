// PNG (bank BANK_PIC): any colour type, depths 1..16, not interlaced, into a 256-colour picture
// (picdec.h frame). Palette images keep their palette, grey ones get 256 greys, true colour (and
// with alpha: the alpha is left out) goes to the colour cube of pic.c with an ordered dither.
// Inflate of the IDAT chunks: the 32 KB history in pages PG_WIN..+1, the previous and the current
// row (unfiltered in place) in page PG_DEC, output bytes gathered in the work RAM first.
#include <string.h>
#include "pic.h"
#include "picdec.h"

#define INB   PIC_WORK                     // 256 bytes of the file
#define ROW   (PIC_WORK + 0x100)           // an output row (up to 360)
#define STG   (PIC_WORK + 0x300)           // inflate output not yet in the history / rows
#define STG_N 512
#define LCNT  ((uint16_t *)(PIC_WORK + 0x500))   // Huffman codes: count per length 0..15
#define LSYM  ((uint16_t *)(PIC_WORK + 0x520))   // symbols by code (288)
#define DCNT  ((uint16_t *)(PIC_WORK + 0x760))
#define DSYM  ((uint16_t *)(PIC_WORK + 0x780))   // (30)
#define LENS  (PIC_WORK + 0x800)                 // code lengths being read (320)
#define OFFS  ((uint16_t *)(PIC_WORK + 0x980))   // (16)
#define ROWMAX 0x2000                            // a raw row: up to 8 KB (PG_DEC holds two)

static uint32_t ip, flen, clen;            // file position, its size, bytes left in this IDAT
static uint16_t inpos, inlen, sn, wpos, rp, rl, prv;
static uint16_t w, h, py;
static uint8_t eod, done, bitbuf, bitcnt, ctype, depth, bpp, ftype, err;

static uint8_t gbyte(void)
{
  if (inpos == inlen) {
    uint16_t n = flen - ip > 256 ? 256 : (uint16_t)(flen - ip);
    if (!n) { eod = 1; return 0; }
    big_read(ip, INB, n);
    ip += n;
    inlen = n;
    inpos = 0;
  }
  return INB[inpos++];
}

static void skip(uint32_t n)
{
  uint16_t k = inlen - inpos;
  if (n <= k) { inpos += (uint16_t)n; return; }
  ip += n - k;
  inpos = inlen = 0;
}

static uint32_t be32(void)
{
  uint32_t v = (uint32_t)gbyte() << 24;
  v |= (uint32_t)gbyte() << 16;
  v |= (uint16_t)gbyte() << 8;
  return v | gbyte();
}

static uint8_t zbyte(void)                 // the next byte of the zlib stream (IDAT chunks in a row)
{
  while (!clen) {
    uint8_t t[4];
    skip(4);                               // the CRC
    clen = be32();
    t[0] = gbyte(); t[1] = gbyte(); t[2] = gbyte(); t[3] = gbyte();
    if (eod || memcmp(t, "IDAT", 4)) { eod = 1; return 0; }
  }
  clen--;
  return gbyte();
}

// ---- rows

static uint8_t paeth(uint8_t a, uint8_t b, uint8_t c)
{
  int16_t p = (int16_t)a + b - c, pa = p - a, pb = p - b, pc = p - c;
  if (pa < 0) pa = -pa;
  if (pb < 0) pb = -pb;
  if (pc < 0) pc = -pc;
  if (pa <= pb && pa <= pc) return a;
  return pb <= pc ? b : c;
}

static const uint8_t bayer[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };

static void row_done(void)                 // PG_DEC is in window 3
{
  uint8_t *cur = WIN3 + (prv ^ ROWMAX), *prev = WIN3 + prv;
  uint16_t i, ox, oy;
  switch (ftype) {
    case 1: for (i = bpp; i < rl; i++) cur[i] += cur[i - bpp]; break;
    case 2: for (i = 0; i < rl; i++) cur[i] += prev[i]; break;
    case 3:
      for (i = 0; i < rl; i++) cur[i] += (uint8_t)(((i >= bpp ? cur[i - bpp] : 0) + (uint16_t)prev[i]) >> 1);
      break;
    case 4:
      for (i = 0; i < rl; i++)
        cur[i] += i >= bpp ? paeth(cur[i - bpp], prev[i], prev[i - bpp]) : prev[i];
      break;
  }
  if (py % fr.f == 0 && (oy = py / fr.f) < fr.oh) {
    uint8_t s = depth == 16 ? 2 : 1;       // 16 bits: the high byte
    for (ox = 0; ox < fr.ow; ox++) {
      uint16_t sx = ox * fr.f;
      uint8_t v;
      if (depth < 8) {                     // palette / grey of 1, 2, 4 bits
        uint16_t bit = sx * depth;
        uint8_t m = (1 << depth) - 1;
        v = (cur[bit >> 3] >> (8 - depth - (bit & 7))) & m;
        if (ctype == 0) v *= 255 / m;
      } else if (ctype == 2 || ctype == 6) {
        uint8_t *p = cur + sx * bpp, t = bayer[(oy & 3) * 4 + (ox & 3)] * 16 + 8;
        v = pic_cube(p[0], p[s], p[2 * s], t);
      } else v = cur[sx * bpp];            // palette / grey / grey + alpha: the first byte
      ROW[ox] = v;
    }
    frame_row(oy, ROW);
    PAGE3(PG_DEC);
  }
  prv ^= ROWMAX;
  if (++py >= h) done = 1;
}

// out bytes into the history and the rows
static void flush(void)
{
  uint16_t i = 0, n;
  while (i < sn) {                         // the history: up to the end of its page
    n = 0x4000 - (wpos & 0x3FFF);
    if (n > sn - i) n = sn - i;
    PAGE3(PG_WIN + (wpos >> 14));
    memcpy(WIN3 + (wpos & 0x3FFF), STG + i, n);
    wpos = (wpos + n) & 0x7FFF;
    i += n;
  }
  PAGE3(PG_DEC);
  for (i = 0; i < sn && !done; i++) {
    if (!rp) ftype = STG[i];
    else WIN3[(prv ^ ROWMAX) + rp - 1] = STG[i];
    if (++rp > rl) { rp = 0; row_done(); }
  }
  sn = 0;
}

static void emit(uint8_t c)
{
  STG[sn++] = c;
  if (sn == STG_N) flush();
}

static void copy(uint16_t len, uint16_t dist)
{
  uint16_t n, src, k, i;
  flush();
  while (len && !done) {
    n = len < dist ? len : dist;
    if (n > STG_N) n = STG_N;
    src = (wpos - dist) & 0x7FFF;
    for (i = 0; i < n; i += k) {
      k = 0x4000 - (src & 0x3FFF);
      if (k > n - i) k = n - i;
      PAGE3(PG_WIN + (src >> 14));
      memcpy(STG + i, WIN3 + (src & 0x3FFF), k);
      src = (src + k) & 0x7FFF;
    }
    sn = n;
    flush();
    len -= n;
  }
}

// ---- inflate

static uint8_t bit(void)
{
  uint8_t b;
  if (!bitcnt) { bitbuf = zbyte(); bitcnt = 8; }
  bitcnt--;
  b = bitbuf & 1;
  bitbuf >>= 1;
  return b;
}

static uint16_t bits(uint8_t n)
{
  uint16_t v = 0;
  uint8_t i;
  for (i = 0; i < n; i++) v |= (uint16_t)bit() << i;
  return v;
}

// canonical Huffman codes of n symbols from their lengths (0: unused)
static void build(uint16_t *cnt, uint16_t *sym, const uint8_t *len, uint16_t n)
{
  uint16_t s;
  uint8_t l;
  memset(cnt, 0, 16 * 2);
  for (s = 0; s < n; s++) cnt[len[s]]++;
  cnt[0] = 0;
  OFFS[1] = 0;
  for (l = 1; l < 15; l++) OFFS[l + 1] = OFFS[l] + cnt[l];
  for (s = 0; s < n; s++) if (len[s]) sym[OFFS[len[s]]++] = s;
}

static uint16_t decode(const uint16_t *cnt, const uint16_t *sym)
{
  uint16_t code = 0, first = 0, index = 0, c;
  uint8_t l;
  for (l = 1; l < 16; l++) {
    code |= bit();
    c = cnt[l];
    if (code - first < c) return sym[index + code - first];
    index += c;
    first = (first + c) << 1;
    code <<= 1;
  }
  err = 1;
  return 256;                              // a broken code: as the end of the block
}

static const uint16_t lbase[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
  35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const uint8_t lext[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
  3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const uint16_t dbase[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
  257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
static const uint8_t dext[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
  7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
static const uint8_t clorder[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

static void codes(void)
{
  uint16_t s, len;
  while (!done && !err && !eod) {
    s = decode(LCNT, LSYM);
    if (s < 256) { emit((uint8_t)s); continue; }
    if (s == 256) return;
    s -= 257;
    if (s >= 29) { err = 1; return; }
    len = lbase[s] + bits(lext[s]);
    s = decode(DCNT, DSYM);
    if (s >= 30) { err = 1; return; }
    copy(len, dbase[s] + bits(dext[s]));
  }
}

static void fixed(void)
{
  uint16_t i;
  for (i = 0; i < 288; i++) LENS[i] = i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8;
  build(LCNT, LSYM, LENS, 288);
  memset(LENS, 5, 30);
  build(DCNT, DSYM, LENS, 30);
}

static void dynamic(void)
{
  uint16_t nlen = bits(5) + 257, ndist = bits(5) + 1, ncode = bits(4) + 4, i, s, rep;
  uint8_t v;
  memset(LENS, 0, 19);
  for (i = 0; i < ncode; i++) LENS[clorder[i]] = (uint8_t)bits(3);
  build(LCNT, LSYM, LENS, 19);
  for (i = 0; i < nlen + ndist && !err;) {
    s = decode(LCNT, LSYM);
    if (s < 16) { LENS[i++] = (uint8_t)s; continue; }
    if (s == 16) { if (!i) { err = 1; break; } v = LENS[i - 1]; rep = 3 + bits(2); }
    else if (s == 17) { v = 0; rep = 3 + bits(3); }
    else { v = 0; rep = 11 + bits(7); }
    if (i + rep > nlen + ndist) { err = 1; break; }
    while (rep--) LENS[i++] = v;
  }
  build(DCNT, DSYM, LENS + nlen, ndist);   // the distance codes first: LENS is reused below
  build(LCNT, LSYM, LENS, nlen);
}

static void inflate(void)
{
  uint8_t last;
  uint16_t n;
  zbyte(); zbyte();                        // the zlib header
  do {
    last = bit();
    switch (bits(2)) {
      case 0:                              // stored
        bitcnt = 0;
        n = zbyte();
        n |= (uint16_t)zbyte() << 8;
        zbyte(); zbyte();
        while (n-- && !done && !eod) emit(zbyte());
        break;
      case 1: fixed(); codes(); break;
      case 2: dynamic(); if (!err) codes(); break;
      default: err = 1;
    }
  } while (!last && !done && !err && !eod);
  flush();
}

uint8_t do_png(uint32_t len, uint8_t buf)
{
  uint8_t t[13], np = 0, i;
  uint32_t n;
  uint16_t k;
  static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 13, 10, 26, 10 };
  ip = 0; flen = len; inpos = inlen = 0; eod = done = err = 0;
  for (i = 0; i < 8; i++) t[i] = gbyte();
  if (memcmp(t, sig, 8)) return PICE_PNG;
  for (;;) {                               // the chunks before the image data
    n = be32();
    for (i = 0; i < 4; i++) t[i] = gbyte();
    if (eod) return PICE_SIZE;
    if (!memcmp(t, "IHDR", 4)) {
      for (i = 0; i < 13; i++) t[i] = gbyte();
      w = t[2] << 8 | t[3];
      h = t[6] << 8 | t[7];
      depth = t[8];
      ctype = t[9];
      if (t[0] || t[1] || t[4] || t[5] || !w || !h || t[12]) return PICE_PNG;   // big / interlaced
      i = ctype == 2 ? 3 : ctype == 4 ? 2 : ctype == 6 ? 4 : 1;   // samples a pixel
      bpp = depth == 16 ? 2 * i : i;
      if (depth > 8 && ctype == 3) return PICE_PNG;
      rl = (uint16_t)(((uint32_t)w * i * depth + 7) / 8);
      if (rl >= ROWMAX) return PICE_PNG;
      skip(4);
    } else if (!memcmp(t, "PLTE", 4)) {
      np = n / 3 > 255 ? 0 : (uint8_t)(n / 3);   // 256 entries: np 0
      for (k = 0; k < n / 3 && k < 256; k++) {
        uint8_t r = gbyte(), g = gbyte(), b = gbyte();
        pal_set(buf, k, pal_rgb(r, g, b));
      }
      skip(n - 3 * k + 4);
    } else if (!memcmp(t, "IDAT", 4)) break;
    else if (!memcmp(t, "IEND", 4)) return PICE_SIZE;
    else skip(n + 4);
  }
  if (!rl) return PICE_PNG;                // no IHDR
  if (ctype == 3) { for (k = np ? np : 256; k < 256; k++) pal_set(buf, k, 0); }
  else if (ctype == 0 || ctype == 4) { for (k = 0; k < 256; k++) pal_set(buf, k, pal_rgb(k, k, k)); }
  else pic_cube_pal(buf);
  if (depth < 8) bpp = 1;                  // the filters work on whole bytes
  frame_begin(buf, w, h, ctype == 3 && np ? 255 : 0);
  clen = n;
  wpos = sn = rp = prv = py = 0;
  bitcnt = 0;
  PAGE3(PG_DEC);
  memset(WIN3, 0, rl);                     // the row before the first: zeros
  inflate();
  return err ? PICE_PNG : PICE_OK;         // a short file: shown as far as it goes
}
