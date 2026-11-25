#include <string.h>
#include "config.h"
#include "ini.h"
#include "fatfs/ff.h"

config_t cfg;
static FATFS fs;

static char ini_buf[2048];
static ini_t ini;
static FIL f;
char zifi_ssid[33], zifi_pass[64];

// a new file: the comments a person editing it on a PC needs
static const char template[] =
  "; karabas gopher settings\r\n"
  "; backend: auto | zifi32 | zifi | usb\r\n"
  "; charset: utf8 | cp1251 | koi8 | cp866\r\n"
  "[net]\r\n"
  "[zifi32]\r\n"
  "[gopher]\r\n";

void cfg_defaults(void) BANKED
{
  memset(&cfg, 0, sizeof(cfg));
  cfg.autoconn = 1;
  strcpy(cfg.home, "gopher://alexspawn.ru");
  strcpy(cfg.downloads, "/downloads");
}

static uint8_t pick(const char *v, const char *const *names, uint8_t n, uint8_t def)
{
  uint8_t i;
  for (i = 0; i < n; i++)
    if (!strcmp(v, names[i])) return i;
  return def;
}

uint8_t cfg_mount(void) BANKED
{
  return f_mount(&fs, "", 1);
}

uint8_t cfg_load(void) BANKED
{
  UINT n;
  char v[8];
  uint8_t r;

  cfg_defaults();
  ini_init(&ini, ini_buf, sizeof(ini_buf), 0);
  r = f_open(&f, CFG_PATH, FA_READ);
  if (r) return r;
  r = f_read(&f, ini_buf, sizeof(ini_buf) - 1, &n);
  f_close(&f);
  if (r) return r;
  ini_init(&ini, ini_buf, sizeof(ini_buf), (uint16_t)n);

  if (ini_get(&ini, "net", "backend", v, sizeof(v)))
    cfg.backend = pick(v, backend_names, BK_COUNT, BK_AUTO);
  if (ini_get(&ini, "gopher", "charset", v, sizeof(v)))
    cfg.charset = pick(v, charset_names, CS_COUNT, CS_UTF8);
  ini_get(&ini, "gopher", "home", cfg.home, sizeof(cfg.home));
  ini_get(&ini, "gopher", "downloads", cfg.downloads, sizeof(cfg.downloads));
  ini_get(&ini, "zifi32", "ssid", cfg.ssid, sizeof(cfg.ssid));
  ini_get(&ini, "zifi32", "password", cfg.pass, sizeof(cfg.pass));
  if (ini_get(&ini, "zifi32", "autoconnect", v, sizeof(v)))
    cfg.autoconn = v[0] == '1';
  return FR_OK;
}

uint8_t cfg_save(void) BANKED
{
  UINT n;
  uint8_t r;

  if (!ini.len) {
    strcpy(ini_buf, template);
    ini_init(&ini, ini_buf, sizeof(ini_buf), sizeof(template) - 1);
  }
  if (!ini_set(&ini, "net", "backend", backend_names[cfg.backend])
      || !ini_set(&ini, "zifi32", "ssid", cfg.ssid)
      || !ini_set(&ini, "zifi32", "password", cfg.pass)
      || !ini_set(&ini, "zifi32", "autoconnect", cfg.autoconn ? "1" : "0")
      || !ini_set(&ini, "gopher", "home", cfg.home)
      || !ini_set(&ini, "gopher", "charset", charset_names[cfg.charset])
      || !ini_set(&ini, "gopher", "downloads", cfg.downloads))
    return FR_NOT_ENOUGH_CORE;

  r = f_mkdir(CFG_DIR);
  if (r && r != FR_EXIST) return r;
  r = f_open(&f, CFG_PATH, FA_WRITE | FA_CREATE_ALWAYS);
  if (r) return r;
  r = f_write(&f, ini_buf, ini.len, &n);
  if (!r && n != ini.len) r = FR_DENIED;
  if (f_close(&f) && !r) r = FR_DISK_ERR;
  return r;
}

static char lower(char c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

// "key: value" or "key = value" of the old client, the key case-insensitive
static void zifi_value(const char *line, const char *key, char *out, uint8_t max)
{
  uint8_t n;
  while (*line == ' ' || *line == '\t') line++;
  for (n = 0; key[n]; n++)
    if (lower(line[n]) != key[n]) return;
  line += n;
  while (*line == ' ' || *line == '\t') line++;
  if (*line != ':' && *line != '=') return;
  line++;
  while (*line == ' ' || *line == '\t') line++;
  for (n = 0; n < max - 1 && line[n] && line[n] != '\r' && line[n] != '\n'; n++) out[n] = line[n];
  while (n && (out[n - 1] == ' ' || out[n - 1] == '\t')) n--;
  out[n] = 0;
}

uint8_t cfg_zifi_read(void) BANKED
{
  UINT n;
  char *p, *e;
  zifi_ssid[0] = zifi_pass[0] = 0;
  if (f_open(&f, ZIFI_INI, FA_READ)) return 0;
  if (f_read(&f, ini_buf, sizeof(ini_buf) - 1, &n)) n = 0;
  f_close(&f);
  ini_buf[n] = 0;
  for (p = ini_buf; *p; p = e) {
    for (e = p; *e && *e != '\n'; e++) ;
    if (*e) *e++ = 0;
    zifi_value(p, "ssid", zifi_ssid, sizeof(zifi_ssid));
    zifi_value(p, "password", zifi_pass, sizeof(zifi_pass));
  }
  ini_init(&ini, ini_buf, sizeof(ini_buf), 0);   // the buffer held ZIFI.INI: no gopher.ini text
  return zifi_ssid[0] != 0;
}
