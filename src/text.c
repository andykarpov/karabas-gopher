// Text screen: TS-Conf text mode 320x240 = 80 x 30 characters, CP866 font, palette 15.
#include <string.h>
#include "ts.h"
#include "text.h"
#include "font866.h"

// PWM levels of the TS-Conf palette (bit 15 = 0): 0..24 per channel
#define C(r, g, b) (uint16_t)(((r) << 10) | ((g) << 5) | (b))
static const uint16_t palette[16] = {
  C(0, 0, 0),    C(0, 0, 16),   C(16, 0, 0),   C(16, 0, 16),
  C(0, 16, 0),   C(0, 16, 16),  C(16, 16, 0),  C(16, 16, 16),
  C(8, 8, 8),    C(0, 0, 24),   C(24, 0, 0),   C(24, 0, 24),
  C(0, 24, 0),   C(0, 24, 24),  C(24, 24, 0),  C(24, 24, 24),
};

static uint8_t cur_x, cur_y, cur_attr = ATTR(COL_WHITE, COL_BLACK);

void txt_init(void)
{
  uint8_t i;
  volatile uint16_t *cram = (volatile uint16_t *)(0xC000 + 15 * 16 * 2);

  TS_PAGE3 = PG_FONT;
  memcpy(WIN3, font866, 2048);

  TS_FMADDR = FM_EN | (0xC000 >> 12);   // window 3 writes go to the CRAM
  for (i = 0; i < 16; i++)
    cram[i] = palette[i];
  TS_FMADDR = 0;

  txt_clear(cur_attr);
  TS_VPAGE = PG_TEXT;
  TS_PALSEL = 0x0F;
  TS_BORDER = 0xF0;                     // colour 0 of palette 15
  TS_TSCONFIG = 0;
  TS_VCONFIG = VID_320X240 | VID_TEXT;
}

void txt_clear(uint8_t attr)
{
  uint8_t y;
  TS_PAGE3 = PG_TEXT;
  for (y = 0; y < TXT_ROWS; y++) {
    memset(WIN3 + y * TXT_LINE, ' ', TXT_COLS);
    memset(WIN3 + y * TXT_LINE + TXT_ATTR, attr, TXT_COLS);
  }
  cur_x = cur_y = 0;
  cur_attr = attr;
}

void txt_goto(uint8_t x, uint8_t y) { cur_x = x; cur_y = y; }
void txt_attr(uint8_t attr) { cur_attr = attr; }

void txt_putc(char c)
{
  uint8_t *p;
  if (c == '\n') { cur_x = 0; cur_y++; return; }
  if (cur_x >= TXT_COLS) { cur_x = 0; cur_y++; }
  if (cur_y >= TXT_ROWS) return;
  TS_PAGE3 = PG_TEXT;
  p = WIN3 + cur_y * TXT_LINE + cur_x;
  p[0] = (uint8_t)c;
  p[TXT_ATTR] = cur_attr;
  cur_x++;
}

void txt_puts(const char *s)
{
  while (*s) txt_putc(*s++);
}
