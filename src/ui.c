#include <string.h>
#include "ui.h"
#include "kbd.h"
#include "edit.h"
#include "net/net.h"

static char old[256];
uint8_t ui_lay_y = TXT_ROWS - 1;

// ---- own glyphs in free codes of the WC font (#01..#1F: only the arrows #18..#1B are used)
// signal: 4 bars over two cells (bars 2, 4, 6, 8 rows high), an unlit bar is its bottom row
static const uint8_t glyphs[][9] = {
  { G_SIG_L,     0, 0, 0, 0, 0, 0, 0, 0x66 },
  { G_SIG_L + 1, 0, 0, 0, 0, 0, 0, 0x60, 0x66 },
  { G_SIG_L + 2, 0, 0, 0, 0, 0x06, 0x06, 0x66, 0x66 },
  { G_SIG_R,     0, 0, 0, 0, 0, 0, 0, 0x66 },
  { G_SIG_R + 1, 0, 0, 0x60, 0x60, 0x60, 0x60, 0x60, 0x66 },
  { G_SIG_R + 2, 0x06, 0x06, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66 },
  { G_LOCK,      0x38, 0x44, 0x44, 0xFE, 0xEE, 0xEE, 0xFE, 0 },
  { G_PAUSE,     0, 0x6C, 0x6C, 0x6C, 0x6C, 0x6C, 0x6C, 0 },
};

void ui_glyphs(void) BANKED
{
  uint8_t i;
  TS_PAGE3 = PG_FONT;
  for (i = 0; i < sizeof(glyphs) / sizeof(glyphs[0]); i++)
    memcpy(WIN3 + glyphs[i][0] * 8, glyphs[i] + 1, 8);
}

// ---- the header: the network and the time at the right of row 0 (docs/PLAN.md 8, variant A)
#define A_HDR(ink) ATTR(ink, COL_BLACK)
static uint16_t t_clock, t_poll;
static uint8_t colon, poll_due = 1;

static void hdr_time(void)
{
  uint8_t h, m;
  char t[6];
  rtc_get(&h, &m);
  if (h > 23 || m > 59) strcpy(t, "--:--");
  else {
    t[0] = '0' + h / 10; t[1] = '0' + h % 10;
    t[3] = '0' + m / 10; t[4] = '0' + m % 10;
    t[5] = 0;
  }
  t[2] = colon ? ':' : ' ';
  txt_print(TXT_COLS - 6, 0, 6, A_HDR(COL_BWHITE), t);
}

static void hdr_net(void)
{
  const char *name = net_lk.ssid;
  uint8_t ink = COL_BGREEN, n;
  txt_fill(HDR_X, 0, TXT_COLS - 6 - HDR_X, ' ', A_HDR(COL_GREY));
  if (!net.active) return;                         // the status row says "нет модуля"
  if (net_lk.state == LK_NONE) { name = "USB"; ink = COL_GREY; }
  else if (net_lk.state == LK_DOWN) { if (!name[0]) name = "нет Wi-Fi"; ink = COL_BRED; }
  else if (net_lk.state == LK_JOINING) ink = COL_BYELLOW;
  n = (uint8_t)strlen(name);
  if (n > HDR_SSID) n = HDR_SSID;
  txt_print(HDR_X + 1 + HDR_SSID - n, 0, n, A_HDR(ink), name);
  if (net_lk.state == LK_UP && net_lk.rssi) {
    int8_t r = net_lk.rssi;
    uint8_t lv = r >= -55 ? 4 : r >= -65 ? 3 : r >= -75 ? 2 : r >= -85 ? 1 : 0;
    txt_fill(HDR_X + HDR_SSID + 2, 0, 1, G_SIG_L + (lv > 2 ? 2 : lv), A_HDR(COL_BGREEN));
    txt_fill(HDR_X + HDR_SSID + 3, 0, 1, G_SIG_R + (lv > 2 ? lv - 2 : 0), A_HDR(COL_BGREEN));
  }
  if (net_lk.state != LK_NONE && net_lk.ecn && net_lk.ecn != 0xFF)
    txt_fill(HDR_X + HDR_SSID + 5, 0, 1, G_LOCK, A_HDR(COL_WHITE));
}

void ui_header(void)
{
  hdr_net();
  hdr_time();
}

void ui_poll_soon(void) { poll_due = 1; }

// between keys: the clock twice a second (the colon blinks), the link every 15 s
static void tick(void)
{
  uint16_t t = ticks;
  if ((uint16_t)(t - t_clock) >= 25) {
    t_clock = t;
    colon ^= 1;
    hdr_time();
  }
  if (net.active && (poll_due || (uint16_t)(t - t_poll) >= 50 * 15)) {
    poll_due = 0;
    net_bg = 1;
    net_link();
    net_bg = 0;
    t_poll = ticks;
    hdr_net();
  }
}

// ---- frames

void ui_layout(void)
{
  txt_print(TXT_COLS - 5, ui_lay_y, 5, A_TITLE, kbd.rus ? " РУС " : " LAT ");
}

void ui_title(const char *title)
{
  txt_fill(0, 0, HDR_X, ' ', A_TITLE);
  txt_print(1, 0, HDR_X - 2, A_TITLE, title);
  ui_header();
}

void ui_frame(const char *title, const char *help)
{
  txt_clear(A_BG);
  ui_title(title);
  txt_fill(0, TXT_ROWS - 1, TXT_COLS, ' ', A_HELP);
  txt_print(1, TXT_ROWS - 1, TXT_COLS - 7, A_HELP, help);
  ui_lay_y = TXT_ROWS - 1;
  ui_layout();
}

void ui_hint(uint8_t x, uint8_t y, uint8_t w, uint8_t a, uint8_t ak, const char *s)
{
  uint8_t key = 0;
  for (; *s && w; s++) {
    if (*s == '~') { key ^= 1; continue; }
    txt_fill(x++, y, 1, *s, key ? ak : a);
    w--;
  }
  if (w) txt_fill(x, y, w, ' ', a);
}

uint16_t ui_key(void)
{
  uint16_t k;
  for (;;) {
    if ((k = kbd_get())) break;
    tick();
    __asm__("halt");
  }
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
