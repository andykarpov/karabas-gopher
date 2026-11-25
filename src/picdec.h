// Inside the picture decoders (bank BANK_PIC: pic.c, gif.c, png.c...): shared helpers of pic.c.
// Window 3 is the only free window while the bank runs: a decoder keeps its tables in page PG_DEC
// (PG_WIN / PG_WIN + 1 for a 32 KB history) and its small buffers in the work RAM at the top of the
// bank (PIC_WORK, above its code: tools/ihxsplit.py checks the code stays below it).
#pragma once
#include <stdint.h>
#ifdef HOST
#include "pic_host_ts.h"
#else
#include "ts.h"
#endif

#define PG_DEC   0x5D                   // a decoder's tables (16 KB)
#define PG_WIN   0x5E                   // 2 pages: a 32 KB history (inflate)
#define PIC_WORK_SIZE 0x1000
#ifdef HOST
extern uint8_t pic_work[PIC_WORK_SIZE];
#define PIC_WORK pic_work
#else
#define PIC_WORK ((uint8_t *)0xB000)    // bank 4: #B000-#BFFF (page #39)
#endif

#define C(r, g, b) (uint16_t)(((uint16_t)(r) << 10) | ((g) << 5) | (b))

void big_read(uint32_t off, uint8_t *out, uint16_t n);
void pal_set(uint8_t buf, uint16_t i, uint16_t w);
uint16_t pal_rgb(uint8_t r, uint8_t g, uint8_t b);     // 8-bit levels -> a CRAM word

// a 256-colour picture of w x h (any size): scaled down by a whole factor to fit 360 x 288, centred
// on the smallest screen that takes it; the rest of the screen is colour `fill`
typedef struct {
  uint16_t w, h, ow, oh, x0, y0;
  uint8_t f, pg;
} frame_t;
extern frame_t fr;
void frame_begin(uint8_t buf, uint16_t w, uint16_t h, uint8_t fill);
void frame_row(uint16_t oy, const uint8_t *row);      // fr.ow bytes to output row oy (< fr.oh)

// true colour into 256 colours: a cube of 6 x 7 x 6 levels (index (r * 7 + g) * 6 + b, the last 4
// entries black), t = the dither threshold 0..255 of the pixel (an ordered dither)
uint8_t pic_cube(uint8_t r, uint8_t g, uint8_t b, uint8_t t);
void pic_cube_pal(uint8_t buf);

uint8_t do_gif(uint32_t len, uint8_t buf);
uint8_t do_png(uint32_t len, uint8_t buf);
