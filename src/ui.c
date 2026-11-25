#include <string.h>
#include "ui.h"
#include "kbd.h"
#include "edit.h"

static char old[256];

void ui_layout(void)
{
  txt_print(TXT_COLS - 5, TXT_ROWS - 1, 5, A_TITLE, kbd.rus ? " РУС " : " LAT ");
}

void ui_frame(const char *title, const char *help)
{
  txt_clear(A_BG);
  txt_fill(0, 0, TXT_COLS, ' ', A_TITLE);
  txt_print(1, 0, TXT_COLS - 2, A_TITLE, title);
  txt_fill(0, TXT_ROWS - 1, TXT_COLS, ' ', A_HELP);
  txt_print(1, TXT_ROWS - 1, TXT_COLS - 7, A_HELP, help);
  ui_layout();
}

uint16_t ui_key(void)
{
  uint16_t k;
  while (!(k = kbd_get())) __asm__("halt");
  if (k == K_INS) { kbd.rus ^= 1; ui_layout(); }
  return k;
}

static void draw_edit(const edit_t *e, uint8_t x, uint8_t y, uint8_t hidden)
{
  uint8_t i, n = e->len - e->scroll;
  if (n > e->width) n = e->width;
  txt_fill(x, y, e->width, ' ', A_EDIT);
  for (i = 0; i < n; i++)
    txt_fill(x + i, y, 1, hidden == 1 ? '*' : e->buf[e->scroll + i], A_EDIT);
  txt_set_attr(x + e->pos - e->scroll, y, A_CUR);
}

uint8_t ui_edit(uint8_t x, uint8_t y, uint8_t w, char *buf, uint8_t max, uint8_t hidden)
{
  edit_t e;
  uint8_t r;
  uint16_t k;
  strcpy(old, buf);
  edit_start(&e, buf, max, w);
  for (;;) {
    draw_edit(&e, x, y, hidden);
    k = ui_key();
    if (k == K_TAB && hidden < 2) { hidden ^= 1; continue; }   // Tab shows / hides a password
    r = edit_key(&e, k);
    if (r == EDIT_DONE) return 1;
    if (r == EDIT_CANCEL) { strcpy(buf, old); return 0; }
  }
}

uint8_t ui_ask(uint8_t y, const char *q)
{
  uint16_t k;
  txt_print(LX, y, TXT_COLS - 2 * LX, A_MSG, q);
  do k = ui_key(); while (k != K_ENTER && k != K_ESC);
  txt_fill(LX, y, TXT_COLS - 2 * LX, ' ', A_BG);
  return k == K_ENTER;
}

void ui_num(char *out, const char *text, int16_t v)
{
  char b[7];
  uint8_t i = 6, neg = v < 0;
  uint16_t u = neg ? (uint16_t)-v : (uint16_t)v;
  b[6] = 0;
  do { b[--i] = '0' + u % 10; u /= 10; } while (u);
  if (neg) b[--i] = '-';
  strcpy(out, text);
  strcat(out, b + i);
}
