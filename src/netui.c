// Ф2 screens (bank BANK_UI): "Сеть" (modules, the backend in use), Wi-Fi (as the boot core:
// status, scan list, hidden network, forget), a test gopher request. Strings are UTF-8 here.
#include <string.h>
#include "ts.h"
#include "ui.h"
#include "kbd.h"
#include "config.h"
#include "settings.h"
#include "netui.h"
#include "net/net.h"

static char pass[65], ssid[33], line[82];
extern gurl_t ui_go;              // browser.c: the address edited here
#define host ui_go.host
#define sel  ui_go.sel
static uint8_t rx[128];

static const char *const bk_title[BK_COUNT] = {
  "", "ZiFi32 (ESP32, SPI)", "ZiFi (ESP8266, UART)", "USB serial (RS232)" };

// ---- Wi-Fi

static const char *join_text(int8_t e)
{
  switch (e) {
    case 1: return "сеть не ответила вовремя";
    case 2: return "неверный пароль";
    case 3: return "сеть не найдена";
    case 4: return "ошибка подключения";
  }
  return "ошибка";
}

static void wifi_status_draw(void)
{
  const at_status_t *s = &net.st;
  txt_print(LX, 2, 20, A_BG, "Модуль");
  strcpy(line, net.active == BK_USB ? "USB serial, AT " : net.active == BK_ZIFI32 ? "ZiFi32 (ESP32), прошивка " : "ESP8266, AT ");
  strcat(line, s->version[0] ? s->version : "?");
  if (net.active == BK_ZIFI32 && net.zf.build[0]) { strcat(line, " "); strcat(line, net.zf.build); }
  txt_print(LX + 20, 2, 58, A_BG, line);
  txt_print(LX, 3, 20, A_BG, net.active == BK_ZIFI32 ? "Сеть хранится" : "Управление потоком");
  if (net.active == BK_ZIFI32)
    txt_print(LX + 20, 3, 50, A_BG, "в " CFG_PATH " (модуль её не помнит)");
  else
    txt_print(LX + 20, 3, 50, s->flow == 2 || s->flow == 3 ? A_BG : A_MSG,
              s->flow < 0 ? "неизвестно" : (s->flow & 2) ? "CTS включён" : "CTS выключен (возможны потери)");
  txt_print(LX, 4, 20, A_BG, "Сеть");
  txt_print(LX + 20, 4, 50, s->ssid[0] ? A_OK : A_MSG, s->ssid[0] ? s->ssid : "не подключено");
  txt_print(LX, 5, 20, A_BG, "IP");
  txt_print(LX + 20, 5, 50, A_BG, s->ip[0] ? s->ip : "-");
}

static void busy(const char *m)
{
  txt_fill(0, TXT_ROWS - 3, TXT_COLS, ' ', A_BG);
  txt_print(LX, TXT_ROWS - 3, TXT_COLS - 2 * LX, A_MSG, m);
}

// joins ssid with a password asked on row y; 1 if joined
static uint8_t join(uint8_t secured, uint8_t y)
{
  uint8_t r;
  pass[0] = 0;
  if (secured) {
    txt_print(LX, y, 20, A_BG, "Пароль");
    txt_print(LX, y + 1, TXT_COLS - 2 * LX, A_DIM, "Tab \xc4 показать / скрыть пароль");
    if (!ui_edit(LX + 20, y, 50, pass, 63, 1)) return 0;
  }
  busy("Подключение... (Esc \xc4 прервать)");
  r = net_join(ssid, pass);
  memset(pass, 0, sizeof(pass));
  if (r == AT_OK) {
    net_status();
    strcpy(line, "Подключено к ");
    strcat(line, ssid);
    busy(line);
    return 1;
  }
  strcpy(line, r == AT_CANCEL ? "Прервано" : r == AT_TIMEOUT ? "Нет ответа модуля" : "Не подключено: ");
  if (r == AT_ERROR) strcat(line, join_text(net.join_err));
  busy(line);
  return 0;
}

#define LIST_Y   3
#define LIST_N   20

static void wifi_list(void)
{
  uint8_t i, s = 0, top = 0, a;
  uint16_t k;
  char l[48];
  for (;;) {
    ui_frame(APP_NAME " \xc4 Wi-Fi: сети", "\x18\x19 сеть   Enter подключиться   R искать снова   Esc назад");
    busy("Поиск сетей...");
    net_scan();
    busy(net.naps ? "\x0b \xc4 сеть с паролем, зелёным \xc4 текущая" : "Сети не найдены (R \xc4 искать снова)");
    for (;;) {
      if (s < top) top = s;
      if (s >= top + LIST_N) top = s - LIST_N + 1;
      for (i = 0; i < LIST_N; i++) {
        uint8_t n = top + i;
        txt_fill(0, LIST_Y + i, TXT_COLS, ' ', A_BG);
        if (n >= net.naps) continue;
        a = n == s ? A_SEL : !strcmp(net.aps[n].ssid, net.st.ssid) ? A_OK : A_BG;
        txt_print(LX, LIST_Y + i, 2, a, net.aps[n].ecn ? "\x0b " : "  ");
        txt_print(LX + 2, LIST_Y + i, 40, a, net.aps[n].ssid);
        ui_num(l, "", net.aps[n].rssi);
        strcat(l, " dBm");
        txt_print(LX + 42, LIST_Y + i, 10, a, l);
      }
      k = ui_key();
      if (k == K_ESC) return;
      if (k == 'r' || k == 'R' || k == 0xAA || k == 0x8A) break;   // R, also к / К of the Russian layout
      if (k == K_UP && s) s--;
      if (k == K_DOWN && s + 1 < net.naps) s++;
      if (k == K_ENTER && net.naps) {
        strcpy(ssid, net.aps[s].ssid);
        txt_fill(0, LIST_Y + LIST_N + 1, TXT_COLS, ' ', A_BG);
        if (join(net.aps[s].ecn, LIST_Y + LIST_N + 1)) { ui_key(); return; }
      }
    }
  }
}

static void wifi_hidden(void)
{
  ui_frame(APP_NAME " \xc4 Wi-Fi: скрытая сеть", "Enter дальше   Esc назад");
  ssid[0] = 0;
  txt_print(LX, 3, 20, A_BG, "Имя сети (SSID)");
  if (!ui_edit(LX + 20, 3, 50, ssid, 32, 2) || !ssid[0]) return;
  txt_print(LX + 20, 3, 50, A_BG, ssid);
  if (join(1, 5)) ui_key();
}

void wifi_run(void) BANKED
{
  uint8_t s = 0, n, r;
  uint16_t k;
  static const char *const items[4] = { "Поиск сетей", "Скрытая сеть", "Забыть сеть", "Перезапустить ZiFi32" };
  for (;;) {
    ui_frame(APP_NAME " \xc4 Wi-Fi", "\x18\x19 пункт   Enter выбрать   Esc назад");
    busy("Опрос модуля...");
    r = net_status();
    busy(r == AT_OK ? "" : "Модуль не отвечает");
    wifi_status_draw();
    // Forget only when joined; Restart only for ZiFi32 (a module left BUSY by an old firmware)
    n = net.st.ssid[0] ? 3 : 2;
    if (net.active == BK_ZIFI32) n++;
    if (s >= n) s = 0;
    for (;;) {
      uint8_t i;
      for (i = 0; i < 4; i++) {
        uint8_t id = i == n - 1 && net.active == BK_ZIFI32 ? 3 : i;
        txt_print(LX, 8 + i * 2, 30, i >= n ? A_BG : i == s ? A_SEL : A_BG, i < n ? items[id] : "");
      }
      k = ui_key();
      if (k == K_ESC) return;
      if (k == K_UP) s = s ? s - 1 : n - 1;
      if (k == K_DOWN) s = s + 1 < n ? s + 1 : 0;
      if (k != K_ENTER) continue;
      if (s == 0) wifi_list();
      else if (s == 1) wifi_hidden();
      else if (net.active == BK_ZIFI32 && s == n - 1) {
        busy("Перезапуск ZiFi32...");
        if (net_zf_reset() == AT_OK) net_use(BK_ZIFI32);
      } else {
        strcpy(line, "Забыть сеть ");
        strcat(line, net.st.ssid);
        strcat(line, "? Модуль перестанет к ней подключаться.  Enter \xc4 да, Esc \xc4 нет");
        if (!ui_ask(15, line)) continue;
        busy("Отключение...");
        net_forget();
      }
      break;
    }
  }
}

// ---- test request

static void host_from_home(void)
{
  const char *p = cfg.home;
  uint8_t i = 0;
  if (!strncmp(p, "gopher://", 9)) p += 9;
  while (*p && *p != '/' && *p != ':' && i < sizeof(host) - 1) host[i++] = *p++;
  host[i] = 0;
}

void nettest_run(void) BANKED
{
  uint8_t r, x = 0, y = 4;
  int16_t n, i;
  uint16_t t0;
  uint32_t total = 0;
  ui_frame(APP_NAME " \xc4 Проверка сети", "Enter запрос   Esc назад");
  host_from_home();
  sel[0] = 0;
  txt_print(LX, 2, 10, A_BG, "Сервер");
  txt_print(LX + 40, 2, 10, A_BG, "Селектор");
  txt_print(LX + 10, 2, 28, A_BG, host);
  if (!ui_edit(LX + 10, 2, 28, host, 63, 2)) return;
  txt_print(LX + 10, 2, 28, A_BG, host);
  if (!ui_edit(LX + 50, 2, 26, sel, 120, 2)) return;
  txt_print(LX + 50, 2, 26, A_BG, sel);
  busy("Соединение... (Esc \xc4 прервать)");
  ui_go.port = 70;
  ui_go.type = '1';
  r = net_gopher_open(&ui_go);
  if (r != AT_OK) {
    busy(r == AT_CANCEL ? "Прервано" : r == NET_NOSEL ? "ZiFi32: селектор должен начинаться с /" : "Нет соединения");
    ui_key();
    return;
  }
  busy("Приём...");
  t0 = ticks;
  while ((n = net_recv(rx, sizeof(rx))) >= 0) {
    if (!n) {
      if ((uint16_t)(ticks - t0) > 50 * 15) { busy("Сервер молчит 15 с"); break; }
      if (kbd_get() == K_ESC) { busy("Прервано"); break; }
      continue;
    }
    t0 = ticks;
    total += (uint16_t)n;
    for (i = 0; i < n && y < TXT_ROWS - 4; i++) {
      uint8_t c = rx[i];
      if (c == '\n') { x = 0; y++; continue; }
      if (c == '\r') continue;
      if (c == '\t') c = ' ';
      if (x < TXT_COLS) txt_fill(x++, y, 1, c < 0x20 ? '.' : c, A_BG);
    }
  }
  net_close();
  ui_num(line, n < 0 ? "Готово, байт: " : "Принято байт: ", (int16_t)(total > 30000 ? 30000 : total));
  busy(line);
  ui_key();
}

// ---- the network screen

void netui_run(void) BANKED
{
  uint8_t s = 0, b, i;
  uint16_t k;
  static const char *const items[5] = {
    "Подключение", "Wi-Fi...", "Проверка: gopher-запрос...", "Найти модули заново", "Настройки..." };
  for (;;) {
    ui_frame(APP_NAME " \xc4 Сеть", "\x18\x19 пункт   Enter выбрать   \x1b\x1a подключение   F2 сохранить   Esc назад");
    txt_print(LX, 2, 30, A_DIM, "Модули");
    for (b = 1; b < BK_COUNT; b++) {
      txt_print(LX, 2 + b, 26, A_BG, bk_title[b]);
      txt_print(LX + 28, 2 + b, 40, net.found[b] ? A_OK : A_DIM,
                b != BK_ZIFI32 && !net.core_zifi ? "нет ZiFi в ядре" :
                net.found[b] ? (net.active == b ? "отвечает, используется" : "отвечает") : "нет ответа");
    }
    for (;;) {
      for (i = 0; i < 5; i++) {
        txt_print(LX, 8 + i * 2, 30, i == s ? A_SEL : A_BG, items[i]);
        if (!i) {
          strcpy(line, backend_names[cfg.backend]);
          if (cfg.backend == BK_AUTO) {
            strcat(line, " \xc4 ");
            strcat(line, net.active ? backend_names[net.active] : "нет модуля");
          }
          txt_print(LX + 32, 8, 40, i == s ? A_SEL : A_BG, line);
        }
      }
      k = ui_key();
      if (k == K_ESC) return;
      if (k == K_UP) s = s ? s - 1 : 4;
      else if (k == K_DOWN) s = s < 4 ? s + 1 : 0;
      else if (s == 0 && (k == K_LEFT || k == K_RIGHT || k == K_ENTER)) {
        cfg.backend = (uint8_t)((cfg.backend + (k == K_LEFT ? BK_COUNT - 1 : 1)) % BK_COUNT);
        net_auto();
        ui_poll_soon();
        break;
      } else if (k == K_F1 + 1) {
        busy(cfg_save() ? "Ошибка записи " CFG_PATH : "Сохранено в " CFG_PATH);
      } else if (k == K_ENTER) {
        if (s == 1) {
          if (net.active) wifi_run();
          else { busy("Нет модуля"); continue; }
        } else if (s == 2) {
          if (net.active) nettest_run();
          else { busy("Нет подключения"); continue; }
        } else if (s == 3) {
          busy("Поиск модулей...");
          net_detect();
          net_auto();
        } else if (s == 4) {
          settings_run();
          net_auto();
        }
        break;
      }
    }
  }
}
