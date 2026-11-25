// Sound Tracker (STC) player for the AY: a blob of its own (ay.h). Written for karabas gopher from
// the format: header delay, positions / ornaments / patterns offsets; 16 samples of 0x63 bytes from
// +0x1B (number, 32 x 3 bytes, loop start, loop length), ornaments of 0x21 bytes (number, 32 note
// offsets), patterns of 7 bytes (number, offsets of channels A, B, C), positions = last index, then
// pairs (pattern, transposition).
#include "ay.h"

static const uint16_t notes[96] = {
  0xef8, 0xe10, 0xd60, 0xc80, 0xbd8, 0xb28, 0xa88, 0x9f0, 0x960, 0x8e0, 0x858, 0x7e0,
  0x77c, 0x708, 0x6b0, 0x640, 0x5ec, 0x594, 0x544, 0x4f8, 0x4b0, 0x470, 0x42c, 0x3f0,
  0x3be, 0x384, 0x358, 0x320, 0x2f6, 0x2ca, 0x2a2, 0x27c, 0x258, 0x238, 0x216, 0x1f8,
  0x1df, 0x1c2, 0x1ac, 0x190, 0x17b, 0x165, 0x151, 0x13e, 0x12c, 0x11c, 0x10b, 0x0fc,
  0x0ef, 0x0e1, 0x0d6, 0x0c8, 0x0bd, 0x0b2, 0x0a8, 0x09f, 0x096, 0x08e, 0x085, 0x07e,
  0x077, 0x070, 0x06b, 0x064, 0x05e, 0x059, 0x054, 0x04f, 0x04b, 0x047, 0x042, 0x03f,
  0x03b, 0x038, 0x035, 0x032, 0x02f, 0x02c, 0x02a, 0x027, 0x025, 0x023, 0x021, 0x01f,
  0x01d, 0x01c, 0x01a, 0x019, 0x017, 0x016, 0x015, 0x013, 0x012, 0x011, 0x010, 0x00f };

typedef struct {
  uint16_t pat, smp, orn, ton;
  uint8_t vol, note, pos, skip;
  int8_t tik, wait;             // tik < 0: the sample is off; wait < 0: the next row is due
  uint8_t env;
} chan_t;

static chan_t ch[3];
static uint8_t delay, transp, position, mixer;

#define POSITIONS W(1)
#define ORNAMENTS W(3)
#define PATTERNS  W(5)

// the offset of the entry numbered n in a table of records of size sz (the number is the first byte)
static uint16_t find(uint16_t at, uint8_t sz, uint8_t n)
{
  uint8_t i = 0;
  while (M(at) != n && --i) at += sz;     // at most 255 records: a broken module stops somewhere
  return at;
}

static void rows(uint8_t pos)
{
  uint16_t p = find(PATTERNS, 7, M(POSITIONS + 1 + 2 * pos));
  transp = M(POSITIONS + 2 + 2 * pos);
  ch[0].pat = W(p + 1);
  ch[1].pat = W(p + 3);
  ch[2].pat = W(p + 5);
}

void ay_init(void)
{
  uint8_t i;
  position = 0;
  delay = 1;
  rows(0);
  for (i = 0; i < 3; i++) {
    chan_t *c = &ch[i];
    c->wait = 0;
    c->env = 0;
    c->skip = 0;
    c->tik = -1;
    c->pos = 0;
    c->orn = ORNAMENTS + 1;
    c->ton = 0;
    c->smp = 0;
    c->note = 0;
    c->vol = 0;
  }
}

static void row(chan_t *c)
{
  uint8_t v;
  for (;;) {
    v = M(c->pat);
    if (v <= 0x5F) {                      // a note
      c->note = v;
      c->tik = 32;
      c->pos = 0;
      c->pat++;
      break;
    }
    if (v <= 0x6F) c->smp = find(0x1B, 0x63, v - 0x60) + 1;
    else if (v <= 0x7F) { c->orn = find(ORNAMENTS, 0x21, v - 0x70) + 1; c->env = 0; }
    else if (v == 0x80) { c->tik = -1; c->pat++; break; }        // rest
    else if (v == 0x81) { c->pat++; break; }                     // empty
    else if (v == 0x82) { c->orn = find(ORNAMENTS, 0x21, 0) + 1; c->env = 0; }
    else if (v <= 0x8E) {                                        // envelope shape + period
      ay_r[R_SHAPE] = v - 0x80;
      ay_env = 1;
      c->pat++;
      ay_r[R_ENV] = M(c->pat);
      c->env = 1;
      c->orn = find(ORNAMENTS, 0x21, 0) + 1;
    } else c->skip = v - 0xA1;
    c->pat++;
  }
  c->wait = c->skip;
}

static void regs(chan_t *c)
{
  uint16_t s;
  uint8_t b0, b1, j;
  if (c->tik >= 0) {
    c->tik--;
    c->pos = (c->pos + 1) & 31;
    if (!c->tik) {
      if (M(c->smp + 0x60)) {             // the sample loops
        c->pos = M(c->smp + 0x60) & 31;
        c->tik = M(c->smp + 0x61) + 1;
      } else c->tik = -1;
    }
  }
  if (c->tik >= 0) {
    s = c->smp + ((c->pos - 1) & 31) * 3;
    b0 = M(s);
    b1 = M(s + 1);
    if (b1 & 0x80) mixer |= 64;           // noise off
    else ay_r[R_NOISE] = b1 & 31;
    if (b1 & 0x40) mixer |= 8;            // tone off
    c->vol = b0 & 15;
    j = c->note + M(c->orn + ((c->pos - 1) & 31)) + transp;
    if (j > 95) j = 95;
    if (b1 & 0x20) c->ton = (notes[j] + M(s + 2) + ((uint16_t)(b0 & 0xF0) << 4)) & 0xFFF;
    else c->ton = (notes[j] - M(s + 2) - ((uint16_t)(b0 & 0xF0) << 4)) & 0xFFF;
    if (c->env) c->vol |= 16;
  } else c->vol = 0;
  mixer >>= 1;
}

void ay_play(void)
{
  uint8_t i;
  if (!--delay) {
    delay = M(0);
    if (--ch[0].wait < 0) {
      if (M(ch[0].pat) == 0xFF) {         // the next position; after the last one the first
        if (position == M(POSITIONS)) { position = 0; ay_flags |= 0x80; }
        else position++;
        rows(position);
      }
      row(&ch[0]);
    }
    if (--ch[1].wait < 0) row(&ch[1]);
    if (--ch[2].wait < 0) row(&ch[2]);
  }
  mixer = 0;
  for (i = 0; i < 3; i++) regs(&ch[i]);
  ay_r[R_MIXER] = mixer;
  for (i = 0; i < 3; i++) {
    ay_r[R_TONA + 2 * i] = (uint8_t)ch[i].ton;
    ay_r[R_TONA_H + 2 * i] = ch[i].ton >> 8;
    ay_r[R_VOLA + i] = ch[i].vol;
  }
}
