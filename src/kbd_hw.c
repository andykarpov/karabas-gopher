// The key buffer of the core: Gluk CMOS (#EFF7 bit 7 = access on, #DFF7 = register, #BFF7 = data).
// Register #0C bit 7 = 0 switches #F0..#FF from the EEPROM to the PS/2 scan code buffer (16 bytes,
// 0 = empty, #FF = overflow), bit 0 = 1 clears the buffer (karabas hid_parser.vhd, Pentevo).
#include "kbd.h"

__sfr __banked __at(0xEFF7) P_EFF7;
__sfr __banked __at(0xDFF7) CMOS_ADDR;
__sfr __banked __at(0xBFF7) CMOS_DATA;

kbd_state_t kbd;

uint8_t kbd_ok;

// karabas: register #0C (bit 7 = 0 buffer instead of the EEPROM, bit 0 = clear) makes #F0 read 0 when
// empty. Pentevo / Unreal: 2 written to #F0..#FF turns the buffer on (0 / 1 = off). The second write
// is made only if the first way left #F0 not empty, so the CMOS of karabas is not written.
void kbd_init(void) BANKED
{
  P_EFF7 = 0x80;
  CMOS_ADDR = 0x0C;
  CMOS_DATA = 0x01;
  CMOS_ADDR = 0xF0;
  if (CMOS_DATA) {
    CMOS_ADDR = 0xF0;
    CMOS_DATA = 0x02;
    CMOS_ADDR = 0xF0;
  }
  kbd_ok = !CMOS_DATA;
  kbd_reset(&kbd);
}

uint16_t kbd_get(void) BANKED
{
  uint8_t c;
  uint16_t k;
  if (!kbd_ok) return K_NONE;
  for (;;) {
    CMOS_ADDR = 0xF0;
    c = CMOS_DATA;
    if (!c) return K_NONE;
    if (c == 0xFF) {          // lost codes: forget the half-read state
      kbd_reset(&kbd);
      CMOS_ADDR = 0x0C;
      CMOS_DATA = 0x01;
      return K_NONE;
    }
    if ((k = kbd_decode(&kbd, c)))
      return k;
  }
}

static uint8_t from_bcd(uint8_t v) { return (v >> 4) * 10 + (v & 15); }

// the clock of the Gluk CMOS (the MCU keeps it, the boot core sets it by NTP): 24-hour h, m
void rtc_get(uint8_t *h, uint8_t *m)
{
  uint8_t b, hh, mm, pm;
  P_EFF7 = 0x80;
  CMOS_ADDR = 0x0B; b = CMOS_DATA;
  CMOS_ADDR = 0x04; hh = CMOS_DATA;
  CMOS_ADDR = 0x02; mm = CMOS_DATA;
  pm = hh & 0x80;
  hh &= 0x7F;
  if (!(b & 4)) { hh = from_bcd(hh); mm = from_bcd(mm); }   // bit 2: binary, else BCD
  if (!(b & 2)) hh = hh % 12 + (pm ? 12 : 0);              // bit 1: 24 hours, else 12 + PM bit
  *h = hh;
  *m = mm;
}

// the date of the Gluk CMOS: year 2000 + y, month, day (0 if the clock has none)
void rtc_date(uint8_t *y, uint8_t *mo, uint8_t *d)
{
  uint8_t b;
  P_EFF7 = 0x80;
  CMOS_ADDR = 0x0B; b = CMOS_DATA;
  CMOS_ADDR = 0x09; *y = CMOS_DATA;
  CMOS_ADDR = 0x08; *mo = CMOS_DATA;
  CMOS_ADDR = 0x07; *d = CMOS_DATA;
  if (!(b & 4)) { *y = from_bcd(*y); *mo = from_bcd(*mo); *d = from_bcd(*d); }
}
