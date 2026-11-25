// The settings screen (Ф1): edits cfg, F2 writes /zifi/gopher.ini. Strings are UTF-8 here,
// the Makefile converts the source to CP866 before compiling.
#include <string.h>
#include "ts.h"
#include "text.h"
#include "kbd.h"
#include "edit.h"
#include "config.h"
#include "settings.h"

#define A_BG     ATTR(COL_WHITE, COL_BLUE)
#define A_TITLE  ATTR(COL_BYELLOW, COL_CYAN)
#define A_SEL    ATTR(COL_BLACK, COL_CYAN)
#define A_EDIT   ATTR(COL_BWHITE, COL_BLACK)
#define A_CUR    ATTR(COL_BLACK, COL_BWHITE)
#define A_HELP   ATTR(COL_BLACK, COL_CYAN)
#define A_MSG    ATTR(COL_BYELLOW, COL_BLUE)

#define LX       2          // label column
#define VX       30         // value column
#define VW       48         // value width
#define Y0       3          // first item row

enum { T_CHOICE, T_TEXT, T_PASS };
typedef struct {
  const char *label;
  uint8_t type;
  void *val;               // uint8_t * (choice) or char * (text)
  uint8_t max;             // text: characters; choice: count
  const char *const *names;
} item_t;


static const item_t items[] = {
  { "Подключение",            T_CHOICE, &cfg.backend,   BK_COUNT, backend_names },
  { "Домашняя страница",      T_TEXT,   cfg.home,       255,      0 },
  { "Кодировка",              T_CHOICE, &cfg.charset,   CS_COUNT, charset_names },
  { "Папка загрузок",         T_TEXT,   cfg.downloads,  63,       0 },
  { "ZiFi32: автоподключение", T_CHOICE, &cfg.autoconn, 2,        yes_no },
};
#define N_ITEMS (sizeof(items) / sizeof(items[0]))

static char msg[80];
static char old[256];
static uint16_t last_key;

static void show_layout(void)
{
  txt_print(TXT_COLS - 5, TXT_ROWS - 1, 5, A_TITLE, kbd.rus ? " РУС " : " LAT ");
}

static void hex16(char *p, uint16_t v)
{
  static const char h[] = "0123456789ABCDEF";
  p[0] = h[v >> 12]; p[1] = h[(v >> 8) & 15]; p[2] = h[(v >> 4) & 15]; p[3] = h[v & 15]; p[4] = 0;
}

static void draw_item(uint8_t i, uint8_t sel)
{
  const item_t *it = &items[i];
  uint8_t y = Y0 + i * 2, a = sel ? A_SEL : A_BG;
  txt_fill(0, y, TXT_COLS, ' ', A_BG);
  txt_print(LX, y, VX - LX - 1, a, it->label);
  if (it->type == T_CHOICE) {
    txt_print(VX, y, VW, a, it->names[*(uint8_t *)it->val]);
  } else if (it->type == T_PASS) {
    uint8_t n = (uint8_t)strlen((char *)it->val);
    txt_fill(VX, y, VW, ' ', a);
    txt_fill(VX, y, n < VW ? n : VW, '*', a);
  } else {
    txt_print(VX, y, VW, a, (char *)it->val);
  }
}

static void draw_status(void)
{
  char k[5];
  txt_print(LX, TXT_ROWS - 4, TXT_COLS - 2 * LX, A_MSG, msg);
  txt_print(LX, TXT_ROWS - 3, 9, A_BG, "Клавиша ");
  hex16(k, last_key);
  txt_print(LX + 9, TXT_ROWS - 3, 4, A_BG, k);
  show_layout();
}

static void draw_all(uint8_t sel)
{
  uint8_t i;
  txt_clear(A_BG);
  txt_fill(0, 0, TXT_COLS, ' ', A_TITLE);
  txt_print(1, 0, 40, A_TITLE, APP_NAME " \xc4 Настройки");
  for (i = 0; i < N_ITEMS; i++) draw_item(i, i == sel);
  txt_fill(0, TXT_ROWS - 1, TXT_COLS, ' ', A_HELP);
  txt_print(1, TXT_ROWS - 1, TXT_COLS - 7, A_HELP,
            "\x18\x19 пункт   Enter изменить   \x1b\x1a выбор   F2 сохранить   Esc назад");
  draw_status();
}

void settings_msg(const char *m, uint8_t code) BANKED
{
  uint8_t n;
  strncpy(msg, m, sizeof(msg) - 14);
  if (code < 100) {
    n = (uint8_t)strlen(msg);
    strcpy(msg + n, " (FatFs   )");
    msg[n + 8] = '0' + code / 10;
    msg[n + 9] = '0' + code % 10;
  }
}

static void draw_edit(const edit_t *e, uint8_t y, uint8_t pass)
{
  uint8_t i, n = e->len - e->scroll;
  if (n > VW) n = VW;
  txt_fill(VX, y, VW, ' ', A_EDIT);
  for (i = 0; i < n; i++) {
    char c = pass ? '*' : e->buf[e->scroll + i];
    txt_fill(VX + i, y, 1, c, A_EDIT);
  }
  txt_set_attr(VX + e->pos - e->scroll, y, A_CUR);
}

static uint16_t wait_key(void)
{
  uint16_t k;
  while (!(k = kbd_get())) __asm__("halt");
  last_key = k;
  if (k == K_INS) { kbd.rus ^= 1; show_layout(); }
  return k;
}

static void edit_item(uint8_t i)
{
  const item_t *it = &items[i];
  uint8_t y = Y0 + i * 2, r;
  edit_t e;
  strcpy(old, (char *)it->val);
  edit_start(&e, (char *)it->val, it->max, VW);
  for (;;) {
    draw_edit(&e, y, it->type == T_PASS);
    draw_status();
    r = edit_key(&e, wait_key());
    if (r == EDIT_CANCEL) strcpy((char *)it->val, old);
    if (r != EDIT_GOING) break;
  }
}

static void cycle(uint8_t i, int8_t d)
{
  uint8_t *v = (uint8_t *)items[i].val, n = items[i].max;
  *v = (uint8_t)((*v + n + d) % n);
}

uint8_t settings_offer_zifi(void) BANKED
{
  uint16_t k;
  draw_all(0xFF);
  txt_print(LX, 20, TXT_COLS - 2 * LX, A_MSG, "Своих настроек нет, но есть " ZIFI_INI " старого клиента:");
  txt_print(LX, 21, 15, A_BG, "сеть Wi-Fi");
  txt_print(LX + 15, 21, 40, A_BG, zifi_ssid);
  txt_print(LX, 23, TXT_COLS - 2 * LX, A_MSG, "Взять из него сеть и пароль?  Enter \xc4 да, Esc \xc4 нет");
  do k = wait_key(); while (k != K_ENTER && k != K_ESC);
  if (k == K_ESC) return 0;
  strcpy(cfg.ssid, zifi_ssid);
  strcpy(cfg.pass, zifi_pass);
  // taken = kept: ZiFi32 joins it on every start from gopher.ini
  if (cfg_save()) settings_msg("Сеть взята из " ZIFI_INI ", но " CFG_PATH " не записан", 0xFF);
  else settings_msg("Сеть взята из " ZIFI_INI " и записана в " CFG_PATH, 0xFF);
  return 1;
}

void settings_run(void) BANKED
{
  uint8_t sel = 0, r;
  uint16_t k;
  draw_all(sel);
  for (;;) {
    k = wait_key();
    msg[0] = 0;
    if (k == K_ESC) return;
    switch (k) {
      case K_UP:   if (sel) sel--; else sel = N_ITEMS - 1; break;
      case K_DOWN: if (++sel == N_ITEMS) sel = 0; break;
      case K_LEFT:
      case K_RIGHT:
        if (items[sel].type == T_CHOICE) cycle(sel, k == K_LEFT ? -1 : 1);
        break;
      case K_ENTER:
        if (items[sel].type == T_CHOICE) cycle(sel, 1);
        else edit_item(sel);
        break;
      case K_F1 + 1:
        settings_msg("Сохранение...", 0xFF);
        draw_status();
        r = cfg_save();
        if (r) settings_msg("Ошибка записи " CFG_PATH, r);
        else settings_msg("Сохранено в " CFG_PATH, 0xFF);
        break;
    }
    draw_all(sel);
  }
}
