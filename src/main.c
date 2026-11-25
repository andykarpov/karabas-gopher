// Ф0: the start-up check. Text mode, a line in Russian (CP866), the frame counter runs.
#include "ts.h"
#include "text.h"

static void put_u16(uint16_t v)
{
  char b[6];
  uint8_t i = 5;
  b[5] = 0;
  do { b[--i] = '0' + v % 10; v /= 10; } while (v && i);
  txt_puts(b + i);
}

void main(void)
{
  txt_init();
  txt_attr(ATTR(COL_BYELLOW, COL_BLUE));
  txt_puts(" karabas gopher \x94\x30 ");                // "Ф0" in CP866
  txt_attr(ATTR(COL_WHITE, COL_BLACK));
  txt_goto(0, 2);
  txt_puts("\x8f\xe0\xa8\xa2\xa5\xe2, TS-Conf!");      // "Привет" in CP866
  __asm__("ei");
  for (;;) {
    txt_goto(0, 4);
    txt_puts("ticks ");
    put_u16(ticks);
    __asm__("halt");
  }
}
