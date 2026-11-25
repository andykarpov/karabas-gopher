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
