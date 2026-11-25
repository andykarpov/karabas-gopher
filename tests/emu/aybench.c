// The C player blobs (stc.bin / asc.bin) on the Z80: make aytest P=stc M=tests/data/ay/stc1.stc
// builds build/aytest.spg with the blob and the module in page #54 (the module at #5200), calls
// INIT and AY_FRAMES x PLAY (crt0 pt3_cmd), hashes the registers of every frame (14 + "13 written")
// and shows "AY OK" if the hash is the one of the reference dump (*.regs, AYFly), else "AY BAD h".
#include "ts.h"
#include "text.h"

extern void pt3_cmd(uint8_t entry);

static char line[24];

void main(void)
{
  uint16_t f, hsh = 0;
  uint8_t i, *r;
  char *p = line;
  txt_init();
  __asm__("ei");
  pt3_cmd(0);
  for (f = 0; f < AY_FRAMES; f++) {
    pt3_cmd(3);
    PAGE3(0x54);
    r = WIN3 + 0x1100;                     // ay_r, ay_env (aycrt.s, linked first at #5100)
    for (i = 0; i < 15; i++) hsh = hsh * 31 + r[i];
  }
  *p++ = 'A'; *p++ = 'Y'; *p++ = ' ';
  if (hsh == AY_HASH) { *p++ = 'O'; *p++ = 'K'; }
  else {
    *p++ = 'B'; *p++ = 'A'; *p++ = 'D'; *p++ = ' ';
    for (i = 0; i < 4; i++) { uint8_t d = (hsh >> 12) & 15; *p++ = d < 10 ? '0' + d : 'A' + d - 10; hsh <<= 4; }
  }
  *p = 0;
  txt_clear(0x1F);
  txt_goto(2, 2);
  txt_attr(0x1F);
  txt_puts(line);
  for (;;) __asm__("halt");
}
