#include <string.h>
#include "ts.h"
#include "ui.h"
#include "kbd.h"
#include "player.h"
#include "gopher/gopher.h"

typedef struct {                        // 256 bytes: 64 a page, 2 pages
  char host[PL_HOST];
  char sel[PL_SEL];
  char name[PL_NAME];
  uint16_t port;
  char type;
} track_t;
#define PL_MAX 128

uint8_t pl_state, pl_count, pl_cur;
static uint16_t pl_t0, pl_played;       // ticks: the start of the current run, frames played before it
static uint8_t pl_loading, last_sec = 0xFF;
static gurl_t pu;                       // RAM: goes to bank 3
static char pname[PL_NAME];

static track_t *entry(uint8_t i)
{
  PAGE3(PG_LIST + (i >> 6));
  return (track_t *)(WIN3 + ((uint16_t)(i & 63) << 8));
}

void player_clear(void) { pl_count = 0; }

uint8_t player_add(const gurl_t *u, const char *name)
{
  track_t *t;
  if (pl_count >= PL_MAX || strlen(u->host) >= PL_HOST || strlen(u->sel) >= PL_SEL) return 0;
  t = entry(pl_count++);
  strcpy(t->host, u->host);
  strcpy(t->sel, u->sel);
  strncpy(t->name, name, PL_NAME - 1);
  t->name[PL_NAME - 1] = 0;
  t->port = u->port;
  t->type = u->type;
  return 1;
}

uint8_t player_playable(const char *sel, uint8_t slen)
{
  const char *e = sel + slen - 4;
  if (slen < 5) return 0;
  return e[0] == '.' && (e[1] | 0x20) == 'p' && (e[2] | 0x20) == 't' && e[3] == '3';
}

static uint16_t elapsed(void)          // seconds
{
  uint16_t f = pl_played;
  if (pl_state == PL_PLAY) f += ticks - pl_t0;
  return f / 50;
}

void player_draw(void)
{
  char b[48], *p = b;
  uint8_t x = 34, w = TXT_COLS - 6 - 11 - x, n;      // the backend name (up to 10) at the right
  if (ui_lay_y != TXT_ROWS - 3) return;       // only on the browser screen (its status row)
  // the keys of the player at the right of the hint row below
  if (pl_state == PL_STOP) txt_fill(TXT_COLS - 34, ui_lay_y + 1, 33, ' ', ATTR(COL_WHITE, COL_BLUE));
  else ui_hint(TXT_COLS - 34, ui_lay_y + 1, 33, ATTR(COL_WHITE, COL_BLUE), ATTR(COL_BYELLOW, COL_BLUE),
               "~F7~ пауза ~F8~ дальше ~Shift+F7~ стоп");
  if (pl_state == PL_STOP && !pl_loading) { txt_fill(x, ui_lay_y, w, ' ', A_HELP); return; }
  *p++ = G_NOTE;
  *p++ = pl_loading ? '.' : pl_state == PL_PLAY ? G_PLAY : G_PAUSE;
  *p++ = ' ';
  ui_num(p, "", pl_cur + 1); p += strlen(p);
  *p++ = '/';
  ui_num(p, "", pl_count); p += strlen(p);
  *p++ = ' ';
  *p = 0;
  n = (uint8_t)(p - b);
  strncpy(p, pname, w - n - 6);
  p[w - n - 6] = 0;
  txt_print(x, ui_lay_y, w, A_HELP, b);
  if (!pl_loading) {
    uint16_t s = elapsed();
    char t[7];
    t[0] = ' ';
    t[1] = '0' + (s / 600) % 10; if (t[1] == '0') t[1] = ' ';
    t[2] = '0' + s / 60 % 10; t[3] = ':';
    t[4] = '0' + s % 60 / 10; t[5] = '0' + s % 10; t[6] = 0;
    txt_print(x + w - 6, ui_lay_y, 6, A_HELP, t);
  }
}

void player_stop(void)
{
  pt3_on = 0;
  pt3_cmd(6);
  pl_state = PL_STOP;
  player_draw();
}

void player_pause(void)
{
  if (pl_state == PL_PLAY) {
    pt3_on = 0;
    pt3_cmd(6);
    pl_played += ticks - pl_t0;
    pl_state = PL_PAUSE;
  } else if (pl_state == PL_PAUSE) {
    pl_t0 = ticks;
    pl_state = PL_PLAY;
    pt3_on = 1;
  }
  player_draw();
}

// n bytes of the loaded module from offset off (the music pages through window 3)
static void mod_read(uint16_t off, uint8_t *out, uint8_t n)
{
  uint16_t a = PT3_MOD - 0x4000 + off;
  while (n--) {
    PAGE3(PG_MUSIC + (a >> 14));
    *out++ = WIN3[a & 0x3FFF];
    a++;
  }
}

// "ProTracker 3." / "Vortex Tracker II" at the start of a PT3 module
static uint8_t is_pt3(uint16_t off)
{
  char m[17];
  mod_read(off, (uint8_t *)m, 17);
  return !memcmp(m, "ProTracker 3.", 13) || !memcmp(m, "Vortex Tracker II", 17);
}

// a TurboSound module ends with "PT3!" len1 "PT3!" len2 "02TS": pt3_mod2 = its second half
static void find_ts(uint16_t len)
{
  uint8_t t[16];
  uint16_t l1;
  pt3_mod2 = 0;
  if (len < 200) return;
  mod_read(len - 16, t, 16);
  if (memcmp(t, "PT3!", 4) || memcmp(t + 6, "PT3!", 4) || memcmp(t + 12, "02TS", 4)) return;
  l1 = t[4] | (uint16_t)t[5] << 8;
  if (l1 < 100 || l1 >= len - 16 || !is_pt3(l1)) return;
  pt3_mod2 = PT3_MOD + l1;
}

uint8_t player_play(uint8_t i)
{
  track_t *t;
  uint8_t r;
  if (i >= pl_count) return G_OK;
  pt3_on = 0;
  pt3_cmd(6);
  pl_cur = i;
  t = entry(i);
  memset(&pu, 0, sizeof(pu));
  strcpy(pu.host, t->host);
  strcpy(pu.sel, t->sel);
  pu.port = t->port;
  pu.type = t->type;
  strcpy(pname, t->name);
  pl_state = PL_STOP;
  pl_loading = 1;
  player_draw();
  r = gopher_load(&pu);
  pl_loading = 0;
  if (r == G_OK && !is_pt3(0)) r = PL_NOTPT3;
  if (r != G_OK) { player_draw(); return r; }
  find_ts((uint16_t)gopher_bytes);
  pt3_end = 0;
  pt3_cmd(0);
  pl_played = 0;
  pl_t0 = ticks;
  pl_state = PL_PLAY;
  pt3_on = 1;
  player_draw();
  return G_OK;
}

uint8_t player_next(int8_t d)
{
  uint8_t i;
  if (!pl_count) return G_OK;
  i = d < 0 ? (pl_cur ? pl_cur - 1 : pl_count - 1) : (pl_cur + 1 < pl_count ? pl_cur + 1 : 0);
  return player_play(i);
}

void player_tick(void)
{
  uint16_t s;
  if (pl_state == PL_PLAY && pt3_end) {
    pt3_end = 0;
    // the next track; a failed one is skipped (at most once round the list)
    {
      uint8_t n = pl_count;
      while (n-- && player_next(1) != G_OK) ;
      if (pl_state != PL_PLAY) player_stop();
    }
    return;
  }
  if (pl_state != PL_PLAY) return;
  s = elapsed();
  if ((uint8_t)s != last_sec) { last_sec = (uint8_t)s; player_draw(); }
}

uint8_t player_key(uint16_t k)
{
  if (k == K_F1 + 6) { player_pause(); return 1; }
  if (k == (K_F1 + 6 | KM_SHIFT)) { player_stop(); return 1; }
  if (k == K_F1 + 7) { if (pl_state != PL_STOP || pl_count) player_next(1); return 1; }
  if (k == (K_F1 + 7 | KM_SHIFT)) { if (pl_state != PL_STOP || pl_count) player_next(-1); return 1; }
  return 0;
}
