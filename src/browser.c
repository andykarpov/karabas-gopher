// Ф3: the gopher browser screen (bank BANK_UI). Menus, text, search, history (page PG_HIST),
// bookmarks (bm.c). Strings are UTF-8 here (the Makefile converts the source to CP866).
#include <string.h>
#include "ts.h"
#include "ui.h"
#include "kbd.h"
#include "config.h"
#include "netui.h"
#include "browser.h"
#include "bm.h"
#include "gopher/gopher.h"
#include "gopher/url.h"
#include "gopher/menu.h"
#include "gopher/cp.h"
#include "net/net.h"

#define PG_HIST   0x3D
#define TOP_Y     1               // first row of the document
#define ROWS      (TXT_ROWS - 3)  // 27 rows: 1..27, row 28 = status, 29 = help
#define STATUS_Y  (TXT_ROWS - 2)
#define TAG_W     5

#define A_INFO   ATTR(COL_WHITE, COL_BLUE)
#define A_LINK   ATTR(COL_BCYAN, COL_BLUE)
#define A_TAG    ATTR(COL_GREY, COL_BLUE)
#define A_ERRL   ATTR(COL_BRED, COL_BLUE)
#define A_STAT   ATTR(COL_BYELLOW, COL_BLUE)

enum { M_MENU, M_TEXT, M_BOOK };

typedef struct {
  gurl_t u;
  uint8_t mode;
  uint16_t top, sel;
} hist_t;

static hist_t cur;                // what is shown
gurl_t ui_go;                     // where to go next (also the address of the test screen)
static char line[300], disp[84], query[80], name[61];
static char link_name[61];      // the text of the link that led to the page shown (bookmark name)
static uint8_t nhist;

// ---- small helpers

static void status(const char *m, uint8_t a)
{
  txt_fill(0, STATUS_Y, TXT_COLS, ' ', A_BG);
  txt_print(1, STATUS_Y, TXT_COLS - 2, a, m);
}

static void title(void)
{
  if (cur.mode == M_BOOK) strcpy(line, "Закладки");
  else url_format(&cur.u, line, TXT_COLS - 2);
  txt_fill(0, 0, TXT_COLS, ' ', A_TITLE);
  txt_print(1, 0, TXT_COLS - 2, A_TITLE, line);
}

static void help(void)
{
  txt_fill(0, TXT_ROWS - 1, TXT_COLS, ' ', A_HELP);
  txt_print(1, TXT_ROWS - 1, TXT_COLS - 7, A_HELP, cur.mode == M_BOOK ?
    "Enter открыть  Del удалить  BS назад  F2 адрес  F3 домой  F9 сеть" :
    "BS назад F2 адрес F3 домой F4 закладки F5 обновить F6 +закладка F9 сеть");
  ui_layout();
}

void browse_progress(void) BANKED
{
  uint32_t k = gopher_bytes >> 10;           // whole KB: no 32-bit printf
  ui_num(disp, "Приём... КБ: ", (int16_t)(k > 30000 ? 30000 : k));
  strcat(disp, "   (Esc \xc4 прервать)");
  status(disp, A_STAT);
}

static const char *tag(char t)
{
  switch (t) {
    case '1': return "DIR";
    case '0': return "TXT";
    case '7': return "FIND";
    case 'h': return "URL";
    case '8': case 'T': return "TEL";
    case 'I': case 'g': case 'p': return "IMG";
    case 's': case '<': return "SND";
    case '3': return "ERR";
    case 'i': return "";
  }
  return "BIN";
}

// ---- rows

static uint8_t row_item(uint16_t i, gitem_t *it)
{
  uint16_t n = gopher_row(i, line, sizeof(line) - 1);
  line[n] = 0;
  if (cur.mode == M_BOOK) {
    // "URL<TAB>name": shown as a menu link of the type in the URL
    char *t = strchr(line, '\t'), *p = strstr(line, "://");
    memset(it, 0, sizeof(*it));
    if (!n) return 0;
    p = strchr(p ? p + 3 : line, '/');
    it->type = p && p[1] && p[1] != '\t' ? p[1] : '1';
    it->disp = t ? t + 1 : line;
    it->dlen = (uint8_t)strlen(it->disp);
    it->sel = line;
    it->slen = (uint8_t)(t ? t - line : n);
    return 1;
  }
  return menu_item(line, n, it);
}

static uint8_t is_link(uint16_t i)
{
  gitem_t it;
  if (cur.mode == M_TEXT) return 0;
  return row_item(i, &it) && menu_link(it.type);
}

static void draw_row(uint16_t i, uint8_t y)
{
  gitem_t it;
  uint8_t sel = cur.mode != M_TEXT && i == cur.sel, a;
  if (i >= gopher_rows()) { txt_fill(0, y, TXT_COLS, ' ', A_BG); return; }
  if (cur.mode == M_TEXT) {
    uint16_t n = gopher_row(i, line, sizeof(line) - 1);
    cp_conv(cfg.charset, line, n, disp, TXT_COLS);
    txt_print(0, y, TXT_COLS, A_INFO, disp);
    return;
  }
  if (!row_item(i, &it)) { txt_fill(0, y, TXT_COLS, ' ', A_BG); return; }
  cp_conv(cur.mode == M_BOOK ? CS_CP866 : cfg.charset, it.disp, it.dlen, disp, TXT_COLS - TAG_W - 1);
  a = sel ? A_SEL : it.type == 'i' ? A_INFO : it.type == '3' ? A_ERRL : A_LINK;
  txt_print(0, y, TAG_W, sel ? A_SEL : A_TAG, tag(it.type));
  txt_print(TAG_W, y, TXT_COLS - TAG_W, a, disp);
}

static void draw_doc(void)
{
  uint8_t y;
  for (y = 0; y < ROWS; y++) draw_row(cur.top + y, TOP_Y + y);
}

static void draw_all(void)
{
  txt_clear(A_BG);
  title();
  help();
  draw_doc();
}

// ---- history (page PG_HIST: hist_t entries)

static void hist_push(void)
{
  hist_t *h = (hist_t *)WIN3;
  uint8_t max = (uint8_t)(0x4000 / sizeof(hist_t));
  TS_PAGE3 = PG_HIST;
  if (nhist == max) { memmove(h, h + 1, (max - 1) * sizeof(hist_t)); nhist--; }
  memcpy(h + nhist++, &cur, sizeof(hist_t));
}

static uint8_t hist_pop(hist_t *out)
{
  if (!nhist) return 0;
  TS_PAGE3 = PG_HIST;
  memcpy(out, (hist_t *)WIN3 + --nhist, sizeof(hist_t));
  return 1;
}

// ---- moving

static void first_link(void)
{
  uint16_t i, rows = gopher_rows();
  cur.sel = 0xFFFF;
  for (i = cur.top; i < rows && i < cur.top + ROWS; i++)
    if (is_link(i)) { cur.sel = i; return; }
}

// Up / Down: the next link on the screen, else a row of scroll (a link coming into view is taken)
static void move(int8_t d)
{
  uint16_t rows = gopher_rows(), i, n;
  if (cur.mode != M_TEXT && cur.sel != 0xFFFF) {
    for (i = cur.sel;;) {
      if (d < 0) { if (!i || i <= cur.top) break; i--; }
      else { if (i + 1 >= rows || i + 1 >= cur.top + ROWS) break; i++; }
      if (is_link(i)) { cur.sel = i; return; }
    }
  }
  if (d < 0 && cur.top) cur.top--;
  else if (d > 0 && cur.top + ROWS < rows) cur.top++;
  else return;
  if (cur.mode == M_TEXT) return;
  n = d > 0 ? cur.top + ROWS - 1 : cur.top;
  if (n < rows && is_link(n)) cur.sel = n;
  else if (cur.sel == 0xFFFF || cur.sel < cur.top || cur.sel >= cur.top + ROWS) first_link();
}

static void page(int8_t d)
{
  uint16_t rows = gopher_rows();
  if (d < 0) cur.top = cur.top > ROWS ? cur.top - ROWS : 0;
  else if (rows > ROWS) cur.top = cur.top + 2 * ROWS < rows ? cur.top + ROWS : rows - ROWS;
  if (cur.mode != M_TEXT) first_link();
}

// ---- opening

static const char *fetch_error(uint8_t r)
{
  switch (r) {
    case G_NONET: return "Нет подключения: F9 \xc4 сеть";
    case G_NOCONN: return "Сервер не отвечает или не найден";
    case G_SEND: return "Ошибка отправки запроса";
    case G_CANCEL: return "Прервано";
    case G_TIMEOUT: return "Сервер замолчал (20 с)";
    case G_NOWIFI: return "ZiFi32 не подключён к Wi-Fi: F9 \xc4 Wi-Fi";
    case G_NOSEL: return "Этот адрес ZiFi32 не открывает: селектор без \"/\" (нужна новая прошивка)";
  }
  return "Ошибка";
}

// shows go: 1 if shown (cur replaced, the old one into the history when push)
static uint8_t open_url(uint8_t push)
{
  uint8_t r, mode = ui_go.type == '0' ? M_TEXT : M_MENU;
  if (ui_go.type != '0' && ui_go.type != '1' && ui_go.type != '7') {
    status(ui_go.type == '8' || ui_go.type == 'T' ? "Telnet не поддерживается" :
           "Загрузка файлов \xc4 в Ф5", A_STAT);
    return 0;
  }
  if (push && cur.u.host[0]) hist_push();
  status("Соединение... (Esc \xc4 прервать)", A_STAT);
  r = gopher_fetch(&ui_go, mode == M_TEXT ? TXT_COLS : 0);
  memcpy(&cur.u, &ui_go, sizeof(gurl_t));
  cur.mode = mode;
  cur.top = 0;
  first_link();
  draw_all();
  if (r != G_OK) {
    strcpy(disp, fetch_error(r));
    if (net.active == BK_ZIFI32 && (r == G_NOCONN || r == G_TIMEOUT)) {
      // the module's own code (docs: B1 DNS, B2 connect, A0 memory...)
      static const char hx[] = "0123456789ABCDEF";
      uint8_t st = net_zf_status(), n = (uint8_t)strlen(disp);
      strcpy(disp + n, " (ZiFi32   )");
      disp[n + 9] = hx[st >> 4];
      disp[n + 10] = hx[st & 15];
    }
    status(disp, A_STAT);
  }
  else if (gopher_full()) status("Документ слишком большой: показано начало", A_STAT);
  else if (!gopher_rows()) status("Пусто", A_STAT);
  else {
    ui_num(disp, "Строк: ", (int16_t)gopher_rows());
    status(disp, A_DIM);
  }
  return 1;
}

static void open_sel(void)
{
  gitem_t it;
  uint8_t i;
  if (cur.mode == M_TEXT || cur.sel == 0xFFFF || !row_item(cur.sel, &it)) return;
  cp_conv(cur.mode == M_BOOK ? CS_CP866 : cfg.charset, it.disp, it.dlen, link_name, sizeof(link_name) - 1);
  if (cur.mode == M_BOOK) {
    memcpy(line, it.sel, it.slen);
    line[it.slen] = 0;
    if (url_parse(line, &ui_go)) open_url(1);
    return;
  }
  if (it.type == 'h' && it.slen > 4 && !strncmp(it.sel, "URL:", 4)) {
    memcpy(disp, it.sel + 4, it.slen - 4 > 70 ? 70 : it.slen - 4);
    disp[it.slen - 4 > 70 ? 70 : it.slen - 4] = 0;
    status(disp, A_STAT);
    return;
  }
  memset(&ui_go, 0, sizeof(ui_go));
  for (i = 0; i < it.hlen && i < URL_HOST - 1; i++) ui_go.host[i] = it.host[i];
  ui_go.port = it.port;
  ui_go.type = it.type;
  memcpy(ui_go.sel, it.sel, it.slen);
  ui_go.sel[it.slen] = 0;
  if (!ui_go.host[0]) memcpy(ui_go.host, cur.u.host, URL_HOST);
  if (it.type == '7') {
    query[0] = 0;
    status("Поиск:", A_STAT);
    if (!ui_edit(8, STATUS_Y, TXT_COLS - 9, query, sizeof(query) - 1, 2) || !query[0]) { status("", A_BG); return; }
    if (strlen(ui_go.sel) + strlen(query) + 2 > URL_SEL) { status("Слишком длинно", A_STAT); return; }
    strcat(ui_go.sel, "\t");
    strcat(ui_go.sel, query);
  }
  open_url(1);
}

static void open_bookmarks(uint8_t push)
{
  uint8_t r;
  if (push && cur.u.host[0]) hist_push();
  r = bm_load();
  cur.mode = M_BOOK;
  cur.top = 0;
  first_link();
  draw_all();
  if (r == 4 /* FR_NO_FILE */ || r == 5 /* FR_NO_PATH */ || !gopher_rows())
    status("Закладок нет: F6 добавляет текущую страницу", A_STAT);
  else if (r) status("Ошибка чтения " BM_PATH, A_STAT);
}

static void add_bookmark(void)
{
  uint8_t r;
  if (cur.mode == M_BOOK || !cur.u.host[0]) return;
  if (link_name[0]) strcpy(name, link_name);
  else url_format(&cur.u, name, sizeof(name));
  status("Название:", A_STAT);
  if (!ui_edit(11, STATUS_Y, TXT_COLS - 12, name, sizeof(name) - 1, 2)) { status("", A_BG); return; }
  url_format(&cur.u, line, 280);
  strcat(line, "\t");
  strcat(line, name);
  r = bm_add(line);
  status(r ? "Ошибка записи " BM_PATH : "Добавлено в закладки", A_STAT);
}

static void ask_url(void)
{
  url_format(&cur.u, line, sizeof(line));
  if (!cur.u.host[0]) strcpy(line, "gopher://");
  status("Адрес:", A_STAT);
  if (!ui_edit(8, STATUS_Y, TXT_COLS - 9, line, 255, 2)) { status("", A_BG); return; }
  if (!url_parse(line, &ui_go)) { status("Неверный адрес", A_STAT); return; }
  link_name[0] = 0;
  open_url(1);
}

static void go_back(void)
{
  hist_t h;
  if (!hist_pop(&h)) { status("Это первая страница", A_DIM); return; }
  link_name[0] = 0;
  if (h.mode == M_BOOK) { open_bookmarks(0); return; }
  memcpy(&ui_go, &h.u, sizeof(gurl_t));
  if (open_url(0)) {
    cur.top = h.top;
    cur.sel = h.sel;
    if (cur.top >= gopher_rows()) cur.top = 0;
    if (cur.mode != M_TEXT && (cur.sel >= gopher_rows() || !is_link(cur.sel))) first_link();
    draw_doc();
  }
}

static void go_home(uint8_t push)
{
  link_name[0] = 0;
  if (url_parse(cfg.home, &ui_go)) open_url(push);
  else status("Домашняя страница не задана: F9 \xc4 сеть \xc4 настройки", A_STAT);
}

void browser_run(void) BANKED
{
  uint16_t k, old_top, old_sel;
  memset(&cur, 0, sizeof(cur));
  cur.sel = 0xFFFF;
  draw_all();
  go_home(0);
  for (;;) {
    k = ui_key();
    old_top = cur.top;
    old_sel = cur.sel;
    switch (k) {
      case K_UP: move(-1); break;
      case K_DOWN: move(1); break;
      case K_PGUP: page(-1); break;
      case K_PGDN: page(1); break;
      case K_HOME: cur.top = 0; if (cur.mode != M_TEXT) first_link(); break;
      case K_END: cur.top = gopher_rows() > ROWS ? gopher_rows() - ROWS : 0; if (cur.mode != M_TEXT) first_link(); break;
      case K_ENTER: case K_RIGHT: open_sel(); continue;
      case K_BS: case K_LEFT: go_back(); continue;
      case K_F1 + 1: ask_url(); continue;
      case K_F1 + 2: go_home(1); continue;
      case K_F1 + 3: open_bookmarks(1); continue;
      case K_F1 + 4:
        if (cur.mode == M_BOOK) open_bookmarks(0);
        else { memcpy(&ui_go, &cur.u, sizeof(gurl_t)); old_top = cur.top; open_url(0); cur.top = old_top; draw_doc(); }
        continue;
      case K_F1 + 5: add_bookmark(); continue;
      case K_DEL:
        if (cur.mode == M_BOOK && cur.sel != 0xFFFF) {
          if (ui_ask(STATUS_Y, "Удалить закладку?  Enter \xc4 да, Esc \xc4 нет")) {
            bm_delete(cur.sel);
            open_bookmarks(0);
          }
        }
        continue;
      case K_F1 + 8:
        netui_run();
        draw_all();
        continue;
      default: continue;
    }
    if (cur.top != old_top) draw_doc();
    else if (cur.sel != old_sel) {
      if (old_sel != 0xFFFF && old_sel >= cur.top && old_sel < cur.top + ROWS) draw_row(old_sel, TOP_Y + old_sel - cur.top);
      if (cur.sel != 0xFFFF) draw_row(cur.sel, TOP_Y + cur.sel - cur.top);
    }
  }
}
