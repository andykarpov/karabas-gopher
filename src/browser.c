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
#include "dl.h"
#include "player.h"
#include "gopher/gopher.h"
#include "gopher/url.h"
#include "gopher/menu.h"
#include "gopher/cp.h"
#include "net/net.h"

// rows (docs/PLAN.md 8): 0 address + header, 1..26 document, 27 status, 28 hints, 29 messages
#define TOP_Y     1
#define ROWS      (TXT_ROWS - 4)
#define STATUS_Y  (TXT_ROWS - 3)
#define HINT_Y    (TXT_ROWS - 2)
#define MSG_Y     (TXT_ROWS - 1)
#define TAG_W     5

#define A_INFO   ATTR(COL_WHITE, COL_BLUE)
#define A_LINK   ATTR(COL_BCYAN, COL_BLUE)
#define A_TAG    ATTR(COL_GREY, COL_BLUE)
#define A_ERRL   ATTR(COL_BRED, COL_BLUE)
#define A_STAT   ATTR(COL_BYELLOW, COL_BLUE)
#define A_BAR    ATTR(COL_BLACK, COL_CYAN)
#define A_BARHI  ATTR(COL_BWHITE, COL_CYAN)
#define A_HINT   ATTR(COL_WHITE, COL_BLUE)
#define A_HINTK  ATTR(COL_BYELLOW, COL_BLUE)

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
static uint8_t nhist, msg_on;
static const char *const bk_shown[BK_COUNT] = { "", "ZiFi32", "ZiFi", "USB" };

// ---- small helpers

// row 29: a message (it goes at the next key), else the keys of the browser
static void status(const char *m, uint8_t a)
{
  txt_fill(0, MSG_Y, TXT_COLS, ' ', A_BG);
  msg_on = m[0] != 0;
  if (msg_on) txt_print(1, MSG_Y, TXT_COLS - 2, a, m);
  else ui_hint(1, MSG_Y, TXT_COLS - 2, A_HINT, A_HINTK, cur.mode == M_BOOK ?
    "~F2~ адрес  ~F3~ домой  ~F9~ сеть" :
    "~F2~ адрес  ~F3~ домой  ~F4~ закладки  ~F5~ обновить  ~F6~ +закладка  ~F9~ сеть");
}

static void title(void)
{
  if (cur.mode == M_BOOK) strcpy(line, "Закладки");
  else url_format(&cur.u, line, HDR_X - 2);
  ui_title(line);
}

// row 27: the mode at the left (the player will go in the middle), the backend and РУС / LAT at the right
static void bar(const char *mode)
{
  const char *b = net.active ? bk_shown[net.active] : "нет модуля";
  uint8_t n = (uint8_t)strlen(b);
  txt_fill(0, STATUS_Y, TXT_COLS, ' ', A_BAR);
  txt_print(1, STATUS_Y, pl_state != PL_STOP ? 32 : 64, A_BAR, mode);
  txt_print(TXT_COLS - 6 - n, STATUS_Y, n, A_BARHI, b);
  ui_lay_y = STATUS_Y;
  ui_layout();
  player_draw();
}

static void bar_view(void)
{
  uint16_t rows = gopher_rows(), last = cur.top + ROWS;
  char nb[12];
  strcpy(disp, cur.mode == M_BOOK ? "Закладки" : cur.mode == M_TEXT ? "Текст" : "Меню");
  if (rows) {
    if (last > rows) last = rows;
    ui_num(nb, "  ", (int16_t)(cur.top + 1));
    strcat(disp, nb);
    ui_num(nb, "\xc4", (int16_t)last);
    strcat(disp, nb);
    ui_num(nb, " из ", (int16_t)rows);
    strcat(disp, nb);
  }
  bar(disp);
}

void browse_progress(void) BANKED
{
  if (gopher_saving >= 2) { player_draw(); return; }   // a track loads: the player part shows it
  uint32_t k = gopher_bytes >> 10;           // whole KB: no 32-bit printf
  uint16_t dt = ticks - gopher_t0;
  char nb[12];
  ui_num(disp, gopher_saving ? "Скачивание: " : "Приём: ", (int16_t)(k > 30000 ? 30000 : k));
  strcat(disp, " КБ");
  if (gopher_saving && dt >= 25) {
    // tenths of KB/s: bytes * 50 / dt / 1024 * 10
    uint16_t v = (uint16_t)(((gopher_bytes >> 8) * 125) / dt);
    ui_num(nb, "  ", (int16_t)(v / 10));
    strcat(disp, nb);
    nb[0] = ','; nb[1] = '0' + v % 10; nb[2] = 0;
    strcat(disp, nb);
    strcat(disp, " КБ/с");
  }
  if (pl_state == PL_STOP) strcat(disp, "   (Esc \xc4 прервать)");
  bar(disp);
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

// row 28: what the selected item does (the player keys will go at the right)
static void hint(void)
{
  gitem_t it;
  const char *h = "~BS~ назад";
  if (cur.mode == M_TEXT) h = "~\x18\x19 PgUp PgDn~ листать  ~F10~ сохранить текст  ~BS~ назад";
  else if (cur.mode == M_BOOK) { if (cur.sel != 0xFFFF) h = "~Enter~ открыть  ~Del~ удалить  ~BS~ назад"; }
  else if (cur.sel != 0xFFFF && row_item(cur.sel, &it))
    switch (it.type) {
      case '1': h = "~Enter~ открыть  ~BS~ назад"; break;
      case '7': h = "~Enter~ искать  ~BS~ назад"; break;
      case 'h': h = "~Enter~ показать адрес  ~BS~ назад"; break;
      case '8': case 'T': h = "telnet не поддерживается  ~BS~ назад"; break;
      case '0': h = "~Enter~ читать  ~F10~ скачать  ~BS~ назад"; break;
      default: {
        uint8_t f = player_format(it.sel, it.slen);
        h = !f ? "~Enter~ / ~F10~ скачать  ~BS~ назад" :
            player_can(f) ? "~Enter~ играть  ~F10~ скачать  ~BS~ назад" :
            f == PF_MOD ? "~F10~ скачать (играть: GS или ZiFi32)" : "~F10~ скачать (играть: ZiFi32 0.6.4+)";
      }
    }
  txt_fill(0, HINT_Y, TXT_COLS, ' ', A_HINT);
  ui_hint(1, HINT_Y, 44, A_HINT, A_HINTK, h);
  player_draw();                             // its keys at the right
}

static void draw_all(void)
{
  txt_clear(A_BG);
  title();
  draw_doc();
  bar_view();
  hint();
  status("", A_BG);
}

// ---- history (page PG_HIST: hist_t entries)

static void hist_push(void)
{
  hist_t *h = (hist_t *)WIN3;
  uint8_t max = (uint8_t)(0x4000 / sizeof(hist_t));
  PAGE3(PG_HIST);
  if (nhist == max) { memmove(h, h + 1, (max - 1) * sizeof(hist_t)); nhist--; }
  memcpy(h + nhist++, &cur, sizeof(hist_t));
}

static uint8_t hist_pop(hist_t *out)
{
  if (!nhist) return 0;
  PAGE3(PG_HIST);
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

// ---- downloads (Ф5 step 2): ui_go into cfg.downloads

static char dlpath[150];          // RAM: goes to banks 3 and 1

static uint8_t downloadable(char t)
{
  return !strchr("17hi38T+", t);
}

static void download(void)
{
  uint8_t r, n;
  char nb[12];
  if (!downloadable(ui_go.type)) { status("Это не файл", A_STAT); return; }
  strcpy(dlpath, cfg.downloads[0] ? cfg.downloads : "/downloads");
  n = (uint8_t)strlen(dlpath);
  if (dlpath[n - 1] != '/') dlpath[n++] = '/';
  {
    // a folder per day: /downloads/2026_11_25/ (the Gluk clock; none if it has no date)
    uint8_t y, mo, d;
    rtc_date(&y, &mo, &d);
    if (y < 100 && mo >= 1 && mo <= 12 && d >= 1 && d <= 31) {
      char *p = dlpath + n;
      *p++ = '2'; *p++ = '0'; *p++ = '0' + y / 10; *p++ = '0' + y % 10; *p++ = '_';
      *p++ = '0' + mo / 10; *p++ = '0' + mo % 10; *p++ = '_';
      *p++ = '0' + d / 10; *p++ = '0' + d % 10; *p++ = '/';
      n += 11;
    }
  }
  url_filename(ui_go.sel, line, 64);
  cp_conv(cfg.charset, line, (uint16_t)strlen(line), dlpath + n, 64);
  for (; dlpath[n]; n++) if (dlpath[n] == '?') dlpath[n] = '_';   // cp_conv marks the unknown by '?'
  status(dlpath, A_DIM);
  bar("Скачивание... (Esc \xc4 прервать)");
  r = gopher_save(&ui_go, dlpath, sizeof(dlpath));
  bar_view();
  if (r == G_OK) {
    strcpy(disp, "Сохранено: ");
    strncat(disp, dlpath, 50);
    if (gopher_bytes < 10240) { ui_num(nb, " (", (int16_t)gopher_bytes); strcat(disp, nb); strcat(disp, " байт)"); }
    else {
      uint32_t k = gopher_bytes >> 10;
      ui_num(nb, " (", (int16_t)(k > 30000 ? 30000 : k)); strcat(disp, nb); strcat(disp, " КБ)");
    }
    status(disp, A_OK);
    return;
  }
  if (r == G_FILE) {
    ui_num(disp, "Не удалось создать файл на SD, ошибка FatFs ", gopher_fr);
    if (gopher_fr == 3 /* FR_NOT_READY */ || gopher_fr == 12 /* FR_NOT_ENABLED */) strcpy(disp, "Нет SD-карты");
  } else if (r == G_WRITE) {
    if (gopher_fr == DL_FULL) strcpy(disp, "SD-карта заполнена: файл удалён");
    else ui_num(disp, "Ошибка записи на SD, файл удалён: FatFs ", gopher_fr);
  } else if (r == G_CANCEL) strcpy(disp, "Скачивание прервано, файл удалён");
  else strcpy(disp, fetch_error(r));
  status(disp, A_STAT);
  if (r == G_NOWIFI || r == G_NOCONN || r == G_TIMEOUT) ui_poll_soon();
}

// shows go: 1 if shown (cur replaced, the old one into the history when push)
static uint8_t open_url(uint8_t push)
{
  uint8_t r, mode = ui_go.type == '0' ? M_TEXT : M_MENU;
  if (ui_go.type != '0' && ui_go.type != '1' && ui_go.type != '7') {
    if (ui_go.type == '8' || ui_go.type == 'T') status("Telnet не поддерживается", A_STAT);
    else if (ui_go.type == 'h') status("Веб-ссылка: браузер её не открывает", A_STAT);
    else download();                         // pictures / music: Ф5 steps 3..5 show / play them
    return 0;
  }
  if (push && cur.u.host[0]) hist_push();
  bar("Соединение... (Esc \xc4 прервать)");
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
    ui_poll_soon();                          // the link may be down: the header shows it soon
  }
  else if (gopher_full()) status("Документ слишком большой: показано начало", A_STAT);
  else if (!gopher_rows()) status("Пусто", A_STAT);
  return 1;
}

// the selected link -> ui_go (and link_name); 0 if there is none (or it is a web URL: shown)
static uint8_t sel_url(void)
{
  gitem_t it;
  uint8_t i;
  if (cur.mode == M_TEXT || cur.sel == 0xFFFF || !row_item(cur.sel, &it)) return 0;
  cp_conv(cur.mode == M_BOOK ? CS_CP866 : cfg.charset, it.disp, it.dlen, link_name, sizeof(link_name) - 1);
  if (cur.mode == M_BOOK) {
    memcpy(line, it.sel, it.slen);
    line[it.slen] = 0;
    return url_parse(line, &ui_go);
  }
  if (it.type == 'h' && it.slen > 4 && !strncmp(it.sel, "URL:", 4)) {
    memcpy(disp, it.sel + 4, it.slen - 4 > 70 ? 70 : it.slen - 4);
    disp[it.slen - 4 > 70 ? 70 : it.slen - 4] = 0;
    status(disp, A_STAT);
    return 0;
  }
  memset(&ui_go, 0, sizeof(ui_go));
  for (i = 0; i < it.hlen && i < URL_HOST - 1; i++) ui_go.host[i] = it.host[i];
  ui_go.port = it.port;
  ui_go.type = it.type;
  memcpy(ui_go.sel, it.sel, it.slen);
  ui_go.sel[it.slen] = 0;
  if (!ui_go.host[0]) memcpy(ui_go.host, cur.u.host, URL_HOST);
  return 1;
}

static const char *play_error(uint8_t r)
{
  if (r == G_BIG) return "Модуль слишком большой: только скачать (F10)";
  if (r == PL_NOTPT3) return "Это не PT3-модуль";
  if (r == PL_NOENG) return "Этот формат здесь не играет: только скачать (F10)";
  if (r == PL_ENGINE) return "GS / ZiFi32 не принял модуль";
  return fetch_error(r);
}

// Enter on a music link: the play list = the music links of this menu, from the selected one
static void play_menu(void)
{
  gitem_t it;
  uint16_t i, rows = gopher_rows();
  uint8_t at = 0, r, k;
  player_stop();
  player_clear();
  for (i = 0; i < rows; i++) {
    if (!row_item(i, &it) || !menu_link(it.type) || !player_format(it.sel, it.slen)) continue;
    memset(&ui_go, 0, sizeof(ui_go));
    for (k = 0; k < it.hlen && k < URL_HOST - 1; k++) ui_go.host[k] = it.host[k];
    if (!ui_go.host[0]) memcpy(ui_go.host, cur.u.host, URL_HOST);
    ui_go.port = it.port;
    ui_go.type = it.type;
    memcpy(ui_go.sel, it.sel, it.slen);
    ui_go.sel[it.slen] = 0;
    cp_conv(cfg.charset, it.disp, it.dlen, name, sizeof(name) - 1);
    if (i == cur.sel) at = pl_count;
    if (!player_add(&ui_go, name)) break;
  }
  bar_view();                                // the status row in the player layout
  r = player_play(at);
  bar_view();
  hint();
  if (r != G_OK) status(play_error(r), A_STAT);
}

static void open_sel(void)
{
  gitem_t it;
  if (cur.mode == M_MENU && cur.sel != 0xFFFF && row_item(cur.sel, &it) && it.type != '1' &&
      it.type != '7' && player_format(it.sel, it.slen)) {
    if (player_can(player_format(it.sel, it.slen))) play_menu();
    else status(play_error(PL_NOENG), A_STAT);
    return;
  }
  if (!sel_url()) return;
  if (cur.mode == M_BOOK) { open_url(1); return; }
  if (ui_go.type == '7') {
    query[0] = 0;
    status("Поиск:", A_STAT);
    if (!ui_edit(8, MSG_Y, TXT_COLS - 9, query, sizeof(query) - 1, 2) || !query[0]) { status("", A_BG); return; }
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
  if (!ui_edit(11, MSG_Y, TXT_COLS - 12, name, sizeof(name) - 1, 2)) { status("", A_BG); return; }
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
  if (!ui_edit(8, MSG_Y, TXT_COLS - 9, line, 255, 2)) { status("", A_BG); return; }
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
    bar_view();
    hint();
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
    if (msg_on) status("", A_BG);
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
        else {
          memcpy(&ui_go, &cur.u, sizeof(gurl_t));
          old_top = cur.top;
          open_url(0);
          if (old_top < gopher_rows()) { cur.top = old_top; if (cur.mode != M_TEXT) first_link(); }
          draw_doc();
          bar_view();
          hint();
        }
        continue;
      case K_F1 + 5: add_bookmark(); continue;
      case K_F1 + 9:
        if (cur.mode == M_TEXT) { memcpy(&ui_go, &cur.u, sizeof(gurl_t)); download(); }
        else if (sel_url()) download();
        continue;
      case K_DEL:
        if (cur.mode == M_BOOK && cur.sel != 0xFFFF) {
          if (ui_ask(MSG_Y, "Удалить закладку?  Enter \xc4 да, Esc \xc4 нет")) {
            bm_delete(cur.sel);
            open_bookmarks(0);
          }
        }
        continue;
      case K_F1 + 8:
        netui_run();
        ui_poll_soon();
        draw_all();
        continue;
      default: continue;
    }
    if (cur.top != old_top) { draw_doc(); bar_view(); }
    else if (cur.sel != old_sel) {
      if (old_sel != 0xFFFF && old_sel >= cur.top && old_sel < cur.top + ROWS) draw_row(old_sel, TOP_Y + old_sel - cur.top);
      if (cur.sel != 0xFFFF) draw_row(cur.sel, TOP_Y + cur.sel - cur.top);
    }
    if (cur.sel != old_sel) hint();
  }
}
