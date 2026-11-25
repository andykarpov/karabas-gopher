// GIF (bank BANK_PIC): the first image of the file into a 256-colour picture (picdec.h frame: scaled
// down by a whole factor to fit, centred). The LZW tables live in page PG_DEC (window 3), the input
// and the output row in the work RAM of the bank; rows go to the picture as they are complete
// (interlaced files too: rows 0, 8.. then 4, 12.. then 2, 6.. then 1, 3..).
#include <string.h>
#include "pic.h"
#include "picdec.h"

#define PREFIX ((uint16_t *)WIN3)          // 4096 codes: the code before (PG_DEC #0000)
#define SUFFIX (WIN3 + 0x2000)             // its last byte (#2000)
#define STACK  (WIN3 + 0x3000)             // a string backwards (#3000, 4 KB)
#define INB    PIC_WORK                    // 256 bytes of the file
#define ROW    (PIC_WORK + 0x100)          // an output row (up to 360)

static uint32_t ip, flen;                  // the next file byte to read into INB, the file size
static uint16_t inpos, inlen;
static uint8_t blk, eod;                   // bytes left in the data sub-block; 1: no more data
static uint16_t w, h, px, py, ox;          // the image, the pixel to come, the output column
static uint8_t xs, pass, ilace, done;
static const uint8_t ipass_y[4] = { 0, 4, 2, 1 }, ipass_s[4] = { 8, 8, 4, 2 };

static uint8_t gbyte(void)
{
  if (inpos == inlen) {
    uint16_t n = flen - ip > 256 ? 256 : (uint16_t)(flen - ip);
    if (!n) { eod = 1; return 0; }
    big_read(ip, INB, n);
    PAGE3(PG_DEC);
    ip += n;
    inlen = n;
    inpos = 0;
  }
  return INB[inpos++];
}

static uint8_t dbyte(void)                 // a byte of the image data (sub-blocks)
{
  if (!blk) {
    blk = gbyte();
    if (!blk) { eod = 1; return 0; }
  }
  blk--;
  return gbyte();
}

static void row_done(void)
{
  if (py % fr.f == 0 && py / fr.f < fr.oh) {
    frame_row(py / fr.f, ROW);
    PAGE3(PG_DEC);
  }
  px = ox = 0;
  xs = 0;
  if (!ilace) { if (++py >= h) done = 1; return; }
  py += ipass_s[pass];
  while (py >= h) {
    if (++pass > 3) { done = 1; return; }
    py = ipass_y[pass];
  }
}

static void put(uint8_t c)
{
  if (!xs) {
    if (ox < fr.ow) ROW[ox] = c;
    ox++;
    xs = fr.f;
  }
  xs--;
  if (++px == w) row_done();
}

static uint8_t lzw(uint8_t mcs)
{
  uint16_t clear = 1u << mcs, next, code, in, old, sp, i;
  uint8_t size, first = 0, nbits = 0, take, got;
  uint16_t acc = 0;                        // bits of the current byte not taken yet (LSB first)
  for (i = 0; i < clear; i++) SUFFIX[i] = (uint8_t)i;
  size = mcs + 1;
  next = clear + 2;
  old = 0xFFFF;
  while (!done) {
    code = 0;                              // a code of `size` bits
    for (got = 0; got < size; got += take) {
      if (!nbits) { acc = dbyte(); nbits = 8; if (eod) return PICE_OK; }   // a short file: as far as it goes
      take = size - got < nbits ? size - got : nbits;
      code |= (acc & ((1u << take) - 1)) << got;
      acc >>= take;
      nbits -= take;
    }
    if (code == clear) { size = mcs + 1; next = clear + 2; old = 0xFFFF; continue; }
    if (code == clear + 1) break;
    if (old == 0xFFFF) {
      if (code >= clear) return PICE_GIF;
      first = (uint8_t)code;
      put(first);
      old = code;
      continue;
    }
    in = code;
    sp = 0;
    if (code >= next) {
      if (code > next) return PICE_GIF;
      STACK[sp++] = first;
      code = old;
    }
    while (code >= clear) {
      if (sp >= 0x1000) return PICE_GIF;
      STACK[sp++] = SUFFIX[code];
      code = PREFIX[code];
    }
    first = (uint8_t)code;
    STACK[sp++] = first;
    if (next < 4096) {
      PREFIX[next] = old;
      SUFFIX[next] = first;
      if (++next == (1u << size) && size < 12) size++;
    }
    old = in;
    while (sp && !done) put(STACK[--sp]);
  }
  return PICE_OK;
}

static uint16_t le16(const uint8_t *p) { return p[0] | (uint16_t)p[1] << 8; }

// a colour table of n entries at the file position: into the palette of buf; the rest black
static void colours(uint8_t buf, uint16_t n)
{
  uint16_t i;
  uint8_t c[3];
  for (i = 0; i < 256; i++) {
    if (i < n) {
      c[0] = gbyte(); c[1] = gbyte(); c[2] = gbyte();
      pal_set(buf, i, pal_rgb(c[0], c[1], c[2]));
      PAGE3(PG_DEC);
    } else pal_set(buf, i, 0);
  }
}

uint8_t do_gif(uint32_t len, uint8_t buf)
{
  uint8_t b[13], f, mcs;
  uint16_t i, ncol = 0, s;
  ip = 0; flen = len; inpos = inlen = 0; eod = 0;
  PAGE3(PG_DEC);
  for (i = 0; i < 13; i++) b[i] = gbyte();
  f = b[10];
  if (eod || memcmp(b, "GIF8", 4)) return PICE_GIF;
  if (f & 0x80) { ncol = 2u << (f & 7); colours(buf, ncol); }
  for (;;) {                               // to the first image
    b[0] = gbyte();
    if (eod) return PICE_SIZE;
    if (b[0] == 0x21) {                    // an extension: its label, then sub-blocks
      gbyte();
      while ((s = gbyte()) != 0 && !eod) while (s--) gbyte();
      continue;
    }
    if (b[0] != 0x2C) return PICE_GIF;
    break;
  }
  for (i = 0; i < 9; i++) b[i] = gbyte();
  w = le16(b + 4);
  h = le16(b + 6);
  f = b[8];
  if (f & 0x80) { ncol = 2u << (f & 7); colours(buf, ncol); }
  if (!ncol || !w || !h) return PICE_GIF;
  ilace = (f & 0x40) != 0;
  mcs = gbyte();
  if (eod || mcs < 2 || mcs > 8) return PICE_GIF;
  frame_begin(buf, w, h, ncol < 256 ? 255 : 0);   // around the image: black (entry 255) if free
  PAGE3(PG_DEC);
  px = py = ox = 0; xs = 0; pass = 0; done = 0; blk = 0;
  return lzw(mcs);
}
