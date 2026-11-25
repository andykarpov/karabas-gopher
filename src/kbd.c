// PS/2 set 2 scan code decoder with the JCUKEN layout (CP866). Portable C (host tests).
#include <string.h>
#include "kbd.h"

// unshifted / shifted US characters by scan code (0 = not a character key)
static const char base[0x67] = {
  [0x0E] = '`', [0x15] = 'q', [0x16] = '1', [0x1A] = 'z', [0x1B] = 's', [0x1C] = 'a', [0x1D] = 'w',
  [0x1E] = '2', [0x21] = 'c', [0x22] = 'x', [0x23] = 'd', [0x24] = 'e', [0x25] = '4', [0x26] = '3',
  [0x29] = ' ', [0x2A] = 'v', [0x2B] = 'f', [0x2C] = 't', [0x2D] = 'r', [0x2E] = '5', [0x31] = 'n',
  [0x32] = 'b', [0x33] = 'h', [0x34] = 'g', [0x35] = 'y', [0x36] = '6', [0x3A] = 'm', [0x3B] = 'j',
  [0x3C] = 'u', [0x3D] = '7', [0x3E] = '8', [0x41] = ',', [0x42] = 'k', [0x43] = 'i', [0x44] = 'o',
  [0x45] = '0', [0x46] = '9', [0x49] = '.', [0x4A] = '/', [0x4B] = 'l', [0x4C] = ';', [0x4D] = 'p',
  [0x4E] = '-', [0x52] = '\'', [0x54] = '[', [0x55] = '=', [0x5B] = ']', [0x5D] = '\\',
};
static const char us_lo[] = "`1234567890-=[]\\;',./";
static const char us_hi[] = "~!@#$%^&*()_+{}|:\"<>?";

// JCUKEN: the US key, the Russian lower and upper case (CP866)
static const char ru_key[] = "qwertyuiop[]asdfghjkl;'zxcvbnm,./`";
static const uint8_t ru_lo[] = {
  0xA9, 0xE6, 0xE3, 0xAA, 0xA5, 0xAD, 0xA3, 0xE8, 0xE9, 0xA7, 0xE5, 0xEA, 0xE4, 0xEB, 0xA2, 0xA0, 0xAF,
  0xE0, 0xAE, 0xAB, 0xA4, 0xA6, 0xED, 0xEF, 0xE7, 0xE1, 0xAC, 0xA8, 0xE2, 0xEC, 0xA1, 0xEE, 0x2E, 0xF1 };
static const uint8_t ru_hi[] = {
  0x89, 0x96, 0x93, 0x8A, 0x85, 0x8D, 0x83, 0x98, 0x99, 0x87, 0x95, 0x9A, 0x94, 0x9B, 0x82, 0x80, 0x8F,
  0x90, 0x8E, 0x8B, 0x84, 0x86, 0x9D, 0x9F, 0x97, 0x91, 0x8C, 0x88, 0x92, 0x9C, 0x81, 0x9E, 0x2C, 0xF0 };

// navigation keys: E0 codes (and the keypad with Num Lock off, the same codes without E0)
static uint16_t nav(uint8_t c)
{
  switch (c) {
    case 0x75: return K_UP;    case 0x72: return K_DOWN;
    case 0x6B: return K_LEFT;  case 0x74: return K_RIGHT;
    case 0x6C: return K_HOME;  case 0x69: return K_END;
    case 0x7D: return K_PGUP;  case 0x7A: return K_PGDN;
    case 0x70: return K_INS;   case 0x71: return K_DEL;
  }
  return K_NONE;
}

static uint16_t fkey(uint8_t c)
{
  static const uint8_t f[12] = { 0x05, 0x06, 0x04, 0x0C, 0x03, 0x0B, 0x83, 0x0A, 0x01, 0x09, 0x78, 0x07 };
  uint8_t i;
  for (i = 0; i < 12; i++)
    if (f[i] == c) return K_F1 + i;
  return K_NONE;
}

void kbd_reset(kbd_state_t *s)
{
  uint8_t rus = s->rus;
  memset(s, 0, sizeof(*s));
  s->rus = rus;
}

static uint16_t mods(const kbd_state_t *s)
{
  return (s->lshift || s->rshift ? KM_SHIFT : 0) | (s->ctrl ? KM_CTRL : 0) | (s->alt ? KM_ALT : 0);
}

uint16_t kbd_decode(kbd_state_t *s, uint8_t code)
{
  uint8_t e0 = s->e0, brk = s->brk, shift;
  uint16_t k;
  const char *p;

  if (code == 0xE0) { s->e0 = 1; return K_NONE; }
  if (code == 0xF0) { s->brk = 1; return K_NONE; }
  s->e0 = s->brk = 0;
  if (code == 0xFF || code == 0x00 || code == 0xE1) return K_NONE;   // overflow, error, Pause

  // modifiers (E0 12 / E0 59 are the fake shifts of Print Screen and the keypad)
  if (code == 0x12 || code == 0x59) {
    if (!e0) { if (code == 0x12) s->lshift = !brk; else s->rshift = !brk; }
    return K_NONE;
  }
  if (code == 0x14) { s->ctrl = !brk; return K_NONE; }
  if (code == 0x11) { s->alt = !brk; return K_NONE; }
  if (brk) return K_NONE;
  if (code == 0x58) { s->caps ^= 1; return K_NONE; }

  if (e0) {
    if (code == 0x5A) return K_ENTER | mods(s);
    if (code == 0x4A) return '/';
    k = nav(code);
    return k ? k | mods(s) : K_NONE;
  }
  switch (code) {
    case 0x5A: return K_ENTER | mods(s);
    case 0x66: return K_BS | mods(s);
    case 0x0D: return K_TAB | mods(s);
    case 0x76: return K_ESC | mods(s);
  }
  if ((k = fkey(code)) || (k = nav(code)))
    return k | mods(s);
  if (code >= sizeof(base) || !base[code]) return K_NONE;

  k = (uint8_t)base[code];
  shift = s->lshift || s->rshift;
  if (s->ctrl || s->alt)
    return k | (s->ctrl ? KM_CTRL : 0) | (s->alt ? KM_ALT : 0) | (shift ? KM_SHIFT : 0);
  if (s->rus && (p = strchr(ru_key, (char)k))) {
    uint8_t i = (uint8_t)(p - ru_key);
    // letters follow Caps Lock, the punctuation of the layout does not
    if (ru_lo[i] >= 0x80) shift ^= s->caps;
    return shift ? ru_hi[i] : ru_lo[i];
  }
  if (k >= 'a' && k <= 'z')
    return (shift ^ s->caps) ? k - 32 : k;
  if (shift && (p = strchr(us_lo, (char)k)))
    return (uint8_t)us_hi[p - us_lo];
  return k;
}
