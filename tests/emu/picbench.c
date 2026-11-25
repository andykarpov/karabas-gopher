// The speed of a picture decoder on the Z80: make bench BENCH=FILE FMT=n (n = PICF_*) builds
// build/bench.spg with FILE in the big buffer (pages #80..); run in unreal-ng (emu-shot): it shows
// the picture for 2 s, then the text screen "r R t F c C": result, frames of the decode (50ths of a
// second), frames of a 27.3 M T-state loop (3.5 MHz: ~390, 14 MHz: ~98).
#include "ts.h"
#include "text.h"
#include "pic.h"

static char line[40];

static char *num(char *p, uint16_t v)
{
  char t[6];
  uint8_t n = 0;
  do { t[n++] = '0' + v % 10; v /= 10; } while (v);
  while (n) *p++ = t[--n];
  return p;
}

void main(void)
{
  uint16_t t0, t, i;
  uint8_t r;
  char *p;
  uint16_t cal;
  txt_init();
  __asm__("ei");
  t0 = ticks;                              // calibration: 16 x 65536 x 26 T = 27.3 M T-states
  __asm
    ld  e, #16
  00101$:
    ld  bc, #0
  00102$:
    dec bc
    ld  a, b
    or  c
    jr  nz, 00102$
    dec e
    jr  nz, 00101$
  __endasm;
  cal = ticks - t0;                        // 3.5 MHz: ~390 frames, 14 MHz: ~98
  t0 = ticks;
  r = pic_decode(BENCH_FMT, BENCH_LEN, 0);
  t = ticks - t0;
  if (!r) {
    pic_show(0);
    for (i = 0; i < 100; i++) __asm__("halt");
    txt_mode();
  }
  p = line;
  *p++ = 'r'; *p++ = ' ';
  p = num(p, r);
  *p++ = ' '; *p++ = 't'; *p++ = ' ';
  p = num(p, t);
  *p++ = ' '; *p++ = 'c'; *p++ = ' ';
  p = num(p, cal);
  *p = 0;
  txt_clear(0x1F);
  txt_goto(2, 2);
  txt_attr(0x1F);
  txt_puts(line);
  for (;;) __asm__("halt");
}
