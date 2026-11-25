// ASC Sound Master player for the AY: a blob of its own (ay.h). Written for karabas gopher from the
// format: header delay, loop position (ASC1 only; ASC0 has none and starts one byte earlier), the
// offsets of the patterns / samples / ornaments tables, the number of positions and the positions.
// The patterns table holds 3 offsets per pattern, the samples / ornaments tables 32 offsets each,
// all relative to their table. A sample row is 3 bytes, an ornament row 2.
#include "ay.h"

static const uint16_t notes[86] = {
  0xedc, 0xe07, 0xd3e, 0xc80, 0xbcc, 0xb22, 0xa82, 0x9ec, 0x95c, 0x8d6, 0x858, 0x7e0,
  0x76e, 0x704, 0x69f, 0x640, 0x5e6, 0x591, 0x541, 0x4f6, 0x4ae, 0x46b, 0x42c, 0x3f0,
  0x3b7, 0x382, 0x34f, 0x320, 0x2f3, 0x2c8, 0x2a1, 0x27b, 0x257, 0x236, 0x216, 0x1f8,
  0x1dc, 0x1c1, 0x1a8, 0x190, 0x179, 0x164, 0x150, 0x13d, 0x12c, 0x11b, 0x10b, 0x0fc,
  0x0ee, 0x0e0, 0x0d4, 0x0c8, 0x0bd, 0x0b2, 0x0a8, 0x09f, 0x096, 0x08d, 0x085, 0x07e,
  0x077, 0x070, 0x06a, 0x064, 0x05e, 0x059, 0x054, 0x050, 0x04b, 0x047, 0x043, 0x03f,
  0x03c, 0x038, 0x035, 0x032, 0x02f, 0x02d, 0x02a, 0x028, 0x026, 0x024, 0x022, 0x020,
  0x01e, 0x01c };

typedef struct {
  uint16_t smp0, smp, smp_loop, orn0, orn, orn_loop, pat, ton, ton_dev;
  int16_t slide, slide_step;
  uint8_t note, note_add, skip, noise0, noise, volume, slide_cnt, vol, vdelay, vdelay_cnt;
  int8_t wait, vol_add;
  uint8_t env, sound, done, no_loop;
} chan_t;

static chan_t ch[3];
static uint8_t h, tempo, delay, position, mixer;   // h: 1 = ASC1 (the loop byte), 0 = ASC0

#define LOOP      (h ? M(1) : 0)
#define PATTERNS  W(1 + h)
#define SAMPLES   W(3 + h)
#define ORNAMENTS W(5 + h)
#define NPOS      M(7 + h)
#define POS(i)    M(8 + h + (i))

static void rows(void)
{
  uint16_t t = PATTERNS, p = t + 6 * POS(position);
  uint8_t i;
  for (i = 0; i < 3; i++) ch[i].pat = W(p + 2 * i) + t;
}

void ay_init(void)
{
  uint8_t *p = (uint8_t *)ch, i;
  int16_t d = (int16_t)W(2) - M(8);
  h = d >= 9 && d <= 72;               // ASC1: the patterns table right after the positions
  for (i = 0; i < sizeof(ch); i++) p[i] = 0;
  position = 0;
  delay = 1;
  tempo = M(0);
  rows();
}

// a slide to the note after the parameter (or by the current slide) in n frames
static void glide(chan_t *c, int16_t d)
{
  int8_t n = (int8_t)M(c->pat);
  if (!n) n = 1;                       // a broken module: no division by 0
  d = d << 4;
  c->slide_step = -d / n;
  c->slide = d - d % n;
  c->slide_cnt = (uint8_t)n;
}

static void row(chan_t *c)
{
  uint8_t v, no_orn = 0, no_smp = 0;
  c->slide_cnt = 0;
  c->vdelay_cnt = 0;
  for (;;) {
    v = M(c->pat);
    if (v <= 0x55) {                   // a note
      c->note = v;
      c->pat++;
      c->noise = c->noise0;
      if ((int8_t)c->slide_cnt <= 0) c->slide = 0;
      if (!no_smp) {
        c->vol_add = 0;
        c->ton_dev = 0;
        c->smp = c->smp0;
        c->sound = 1;
        c->done = 0;
        c->no_loop = 0;
      }
      if (!no_orn) {
        c->orn = c->orn0;
        c->note_add = 0;
      }
      if (c->env) ay_r[R_ENV] = M(c->pat++);
      break;
    }
    if (v <= 0x5D) { c->pat++; break; }
    if (v == 0x5E) { c->no_loop = 1; c->pat++; break; }
    if (v == 0x5F) { c->sound = 0; c->pat++; break; }
    if (v <= 0x9F) c->skip = v - 0x60;
    else if (v <= 0xBF) c->smp0 = W(SAMPLES + (v - 0xA0) * 2) + SAMPLES;
    else if (v <= 0xDF) c->orn0 = W(ORNAMENTS + (v - 0xC0) * 2) + ORNAMENTS;
    else if (v == 0xE0) { c->volume = 15; c->env = 1; }
    else if (v <= 0xEF) { c->volume = v - 0xE0; c->env = 0; }
    else switch (v) {
      case 0xF0: c->noise0 = M(++c->pat); break;
      case 0xF1: no_smp = 1; break;
      case 0xF2: no_orn = 1; break;
      case 0xF3: no_smp = no_orn = 1; break;
      case 0xF4: tempo = M(++c->pat); break;
      case 0xF5: c->slide_step = -(int8_t)M(++c->pat) * 16; c->slide_cnt = 255; break;
      case 0xF6: c->slide_step = (int8_t)M(++c->pat) * 16; c->slide_cnt = 255; break;
      case 0xF7:
        c->pat++;
        no_smp = 1;
        glide(c, M(c->pat + 1) < 0x56 ? notes[c->note] + c->slide / 16 - notes[M(c->pat + 1)] : c->slide / 16);
        break;
      case 0xF8: ay_r[R_SHAPE] = 8; ay_env = 1; break;
      case 0xF9:
        c->pat++;
        glide(c, M(c->pat + 1) < 0x56 ? notes[c->note] - notes[M(c->pat + 1)] : c->slide / 16);
        break;
      case 0xFA: ay_r[R_SHAPE] = 10; ay_env = 1; break;
      case 0xFB:
        v = M(++c->pat);
        c->vdelay = v & 32 ? ((uint8_t)(v << 3) ^ 0xF8) + 9 : v << 3;
        c->vdelay_cnt = c->vdelay;
        break;
      case 0xFC: ay_r[R_SHAPE] = 12; ay_env = 1; break;
      case 0xFE: ay_r[R_SHAPE] = 14; ay_env = 1; break;
    }
    c->pat++;
  }
  c->wait = c->skip;
}

static void regs(chan_t *c)
{
  uint8_t s0, s1, s2, o0, ok_env;
  int8_t j;
  if (c->done || !c->sound) { c->vol = 0; mixer >>= 1; return; }
  if (c->vdelay_cnt) {                 // the volume slides by itself
    if (c->vdelay_cnt >= 16) {
      c->vdelay_cnt -= 8;
      if (c->vol_add < -15) c->vol_add++;
      else if (c->vol_add > 15) c->vol_add--;
    } else {
      if (c->vdelay_cnt & 1) { if (c->vol_add > -15) c->vol_add--; }
      else if (c->vol_add < 15) c->vol_add++;
      c->vdelay_cnt = c->vdelay;
    }
  }
  s0 = M(c->smp);
  s1 = M(c->smp + 1);
  s2 = M(c->smp + 2);
  if (s0 & 128) c->smp_loop = c->smp;
  if ((s0 & 96) == 32) c->done = 1;
  c->ton_dev += (int8_t)s1;
  mixer |= (s2 & 9) << 3;
  ok_env = (s2 & 6) == 2;
  if ((s2 & 6) == 4 && c->vol_add > -15) c->vol_add--;
  if ((s2 & 6) == 6 && c->vol_add < 15) c->vol_add++;
  c->vol = c->vol_add + (s2 >> 4);
  if ((int8_t)c->vol < 0) c->vol = 0;
  else if (c->vol > 15) c->vol = 15;
  c->vol = (uint8_t)((c->vol * (c->volume + 1)) >> 4);
  if (ok_env && (mixer & 64)) ay_r[R_ENV] += (int8_t)(s0 << 3) / 8;
  else c->noise += (int8_t)(s0 << 3) / 8;
  c->smp += 3;
  if (s0 & 64) {
    if (!c->no_loop) c->smp = c->smp_loop;
    else if (s0 & 32) c->done = 1;
  }
  o0 = M(c->orn);
  if (o0 & 128) c->orn_loop = c->orn;
  c->note_add += M(c->orn + 1);
  c->noise += (uint8_t)(-(int8_t)(o0 & 0x10) | o0);
  c->orn += 2;
  if (o0 & 64) c->orn = c->orn_loop;
  if (!(mixer & 64)) ay_r[R_NOISE] = ((uint8_t)(c->slide >> 8) + c->noise) & 31;
  j = (int8_t)(c->note + c->note_add);
  if (j < 0) j = 0;
  else if (j > 0x55) j = 0x55;
  c->ton = (notes[j] + c->ton_dev + c->slide / 16) & 0xFFF;
  if (c->slide_cnt) {
    if ((int8_t)c->slide_cnt > 0) c->slide_cnt--;
    c->slide += c->slide_step;
  }
  if (c->env && ok_env) c->vol |= 16;
  mixer >>= 1;
}

void ay_play(void)
{
  uint8_t i;
  if (!--delay) {
    if (--ch[0].wait < 0) {
      if (M(ch[0].pat) == 0xFF) {        // the next position; after the last one the loop
        if (++position >= NPOS) { position = LOOP; ay_flags |= 0x80; }
        rows();
        ch[0].noise0 = ch[1].noise0 = ch[2].noise0 = 0;
      }
      row(&ch[0]);
    }
    if (--ch[1].wait < 0) row(&ch[1]);
    if (--ch[2].wait < 0) row(&ch[2]);
    delay = tempo;
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
