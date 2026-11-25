#include <string.h>
#include "ts.h"
#include "ui.h"
#include "kbd.h"
#include "player.h"
#include "gopher/gopher.h"
#include "net/net.h"

typedef struct {                        // 256 bytes: 64 a page, 2 pages
  char host[PL_HOST];
  char sel[PL_SEL];
  char name[PL_NAME];
  uint16_t port;
  char type;
  uint16_t row;                         // play list: the row of the link in its menu
} track_t;

uint8_t pl_state, pl_count, pl_cur, pl_moved;
uint16_t pl_src;
static uint16_t pl_t0, pl_played;       // ticks: the start of the current run, frames played before it
static uint8_t pl_loading, pl_trk, last_sec = 0xFF;   // pl_trk: the track plays on the GS / ZiFi32
static gurl_t pu;                       // RAM: goes to bank 3
static char pname[PL_NAME];

static track_t *entry(uint8_t pg, uint8_t i)
{
  PAGE3(pg + (i >> 6));
  return (track_t *)(WIN3 + ((uint16_t)(i & 63) << 8));
}

// ---- lists of links (the play list, the pictures of a menu): 256-byte entries, 2 pages from pg

uint8_t mlist_put(uint8_t pg, uint8_t i, const gurl_t *u, const char *name)
{
  track_t *t;
  if (i >= PL_MAX || strlen(u->host) >= PL_HOST || strlen(u->sel) >= PL_SEL) return 0;
  t = entry(pg, i);
  strcpy(t->host, u->host);
  strcpy(t->sel, u->sel);
  strncpy(t->name, name, PL_NAME - 1);
  t->name[PL_NAME - 1] = 0;
  t->port = u->port;
  t->type = u->type;
  return 1;
}

void mlist_get(uint8_t pg, uint8_t i, gurl_t *u, char *name)
{
  track_t *t = entry(pg, i);
  memset(u, 0, sizeof(*u));
  strcpy(u->host, t->host);
  strcpy(u->sel, t->sel);
  u->port = t->port;
  u->type = t->type;
  strcpy(name, t->name);
}

void player_clear(void) { pl_count = 0; }

uint8_t player_add(const gurl_t *u, const char *name, uint16_t row)
{
  if (!mlist_put(PG_LIST, pl_count, u, name)) return 0;
  entry(PG_LIST, pl_count)->row = row;
  pl_count++;
  return 1;
}

uint16_t player_row(uint8_t i) { return entry(PG_LIST, i)->row; }

static uint8_t ext(const char *e, const char *x)   // x: lower case
{
  for (; *x; e++, x++) if ((*e | 0x20) != *x) return 0;
  return 1;
}

uint8_t player_format(const char *sel, uint8_t slen)
{
  const char *t = sel + slen;
  if (slen < 4) return PF_NONE;
  if (t[-4] == '.' && ext(t - 3, "pt3")) return PF_PT3;
  if (t[-4] == '.' && ext(t - 3, "pt2")) return PF_PT2;
  if (t[-4] == '.' && ext(t - 3, "stc")) return PF_STC;
  if (t[-4] == '.' && ext(t - 3, "asc")) return PF_ASC;
  if (t[-4] == '.' && ext(t - 3, "mod")) return PF_MOD;
  if (t[-4] == '.' && ext(t - 3, "s3m")) return PF_S3M;
  if (t[-3] == '.' && ext(t - 2, "xm")) return PF_XM;
  return PF_NONE;
}

uint8_t player_can(uint8_t fmt)
{
  if (PF_AY(fmt)) return 1;
  if (fmt == PF_NONE) return 0;
  return trk_engine(fmt - PF_MOD) != TRK_NONE;
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
  if (!pl_loading) {                         // who plays it
    uint8_t e = pl_trk ? trk_now() : 0;
    strcpy(p, e == TRK_GS ? "GS " : e == TRK_ZF ? "Z32 " : "AY ");
    p += strlen(p);
  }
  *p = 0;
  n = (uint8_t)(p - b);
  strncpy(p, pname, w - n - 6);
  p[w - n - 6] = 0;
  txt_print(x, ui_lay_y, w, A_HELP, b);
  if (pl_loading) {                           // the size loaded so far
    char t[8];
    ui_num(t, " ", (int16_t)(gopher_bytes >> 10));
    strcat(t, "K");
    txt_print(x + w - 6, ui_lay_y, 6, A_HELP, t);
  } else {
    uint16_t s = elapsed();
    char t[7];
    t[0] = ' ';
    t[1] = '0' + (s / 600) % 10; if (t[1] == '0') t[1] = ' ';
    t[2] = '0' + s / 60 % 10; t[3] = ':';
    t[4] = '0' + s % 60 / 10; t[5] = '0' + s % 10; t[6] = 0;
    txt_print(x + w - 6, ui_lay_y, 6, A_HELP, t);
  }
}

static uint8_t blob_now = 0xFF;         // the player blob in PG_MUSIC (0xFF: none yet)

static void silence(void)
{
  pt3_on = 0;
  if (blob_now != 0xFF) pt3_cmd(6);
  if (pl_trk) { trk_stop(); pl_trk = 0; }
}

void player_stop(void)
{
  silence();
  pl_state = PL_STOP;
  player_draw();
}

void player_pause(void)
{
  if (pl_state == PL_PLAY) {
    if (pl_trk) trk_pause(1);
    else { pt3_on = 0; pt3_cmd(6); }
    pl_played += ticks - pl_t0;
    pl_state = PL_PAUSE;
  } else if (pl_state == PL_PAUSE) {
    pl_t0 = ticks;
    pl_state = PL_PLAY;
    if (pl_trk) trk_pause(0);
    else pt3_on = 1;
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

// the player blob n of PG_PLAYERS into PG_MUSIC at #4000 (the interrupt does not play meanwhile)
static void blob_load(uint8_t n)
{
  uint8_t b[64];
  uint16_t off, len, i;
  if (blob_now == n) return;
  PAGE3(PG_PLAYERS);
  off = *(uint16_t *)(WIN3 + 4 * n);
  len = *(uint16_t *)(WIN3 + 4 * n + 2);
  for (i = 0; i < len; i += sizeof(b)) {
    PAGE3(PG_PLAYERS);
    memcpy(b, WIN3 + off + i, sizeof(b));
    PAGE3(PG_MUSIC);
    memcpy(WIN3 + i, b, sizeof(b));
  }
  blob_now = n;
}

// a PT2 module: delay, positions, loop position, 32 sample / 16 ornament pointers, the patterns
// pointer (+99), the name, the position list from +131 ending with #FF (the player needs that)
static uint8_t is_pt2(uint16_t len)
{
  uint8_t h[3], e;
  uint16_t pat;
  if (len < 132 + 1) return 0;
  mod_read(0, h, 3);
  mod_read(99, (uint8_t *)&pat, 2);
  if (!h[0] || !h[1] || h[2] >= h[1] || pat >= len || 131 + h[1] >= len) return 0;
  mod_read(131 + h[1], &e, 1);
  return e == 0xFF;
}

static uint8_t is_pt3(uint16_t off);

static uint16_t mod_word(uint16_t off)
{
  uint16_t w;
  mod_read(off, (uint8_t *)&w, 2);
  return w;
}

// STC: the positions / ornaments / patterns offsets inside the module, a delay
// ASC: the patterns / samples / ornaments offsets inside it (ASC1: after the loop byte, ASC0: none)
static uint8_t is_module(uint8_t fmt, uint16_t len)
{
  uint8_t d, h = 0;
  if (fmt == PF_PT3) return is_pt3(0);
  if (fmt == PF_PT2) return is_pt2(len);
  if (len < 32) return 0;
  mod_read(0, &d, 1);
  if (fmt == PF_STC)
    return d && mod_word(1) < len && mod_word(3) < len && mod_word(5) < len;
  {
    uint8_t n;
    int16_t x;
    mod_read(8, &n, 1);
    x = (int16_t)mod_word(2) - n;
    h = x >= 9 && x <= 72;
  }
  return mod_word(1 + h) < len && mod_word(3 + h) < len && mod_word(5 + h) < len;
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
  uint8_t r;
  uint8_t fmt;
  if (i >= pl_count) return G_OK;
  silence();
  pl_cur = i;
  pl_moved = 1;
  mlist_get(PG_LIST, i, &pu, pname);
  fmt = player_format(pu.sel, (uint8_t)strlen(pu.sel));
  pl_state = PL_STOP;
  if (!player_can(fmt)) { player_draw(); return PL_NOENG; }
  pl_loading = 1;
  player_draw();
  if (!PF_AY(fmt)) {                         // a tracker module: the big buffer, then the GS / ZiFi32
    r = gopher_load_big(&pu);
    if (r == G_OK && trk_play(fmt - PF_MOD, gopher_bytes)) r = PL_ENGINE;
    pl_loading = 0;
    if (r != G_OK) { player_draw(); return r; }
    pl_trk = 1;
    pl_played = 0;
    pl_t0 = ticks;
    pl_state = PL_PLAY;
    player_draw();
    return G_OK;
  }
  r = gopher_load(&pu);
  pl_loading = 0;
  if (r == G_OK && !is_module(fmt, (uint16_t)gopher_bytes)) r = PL_NOTPT3;
  if (r != G_OK) { player_draw(); return r; }
  pt3_mod2 = 0;
  if (fmt == PF_PT3) find_ts((uint16_t)gopher_bytes);
  blob_load(fmt - PF_PT3);
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
  if ((uint8_t)s != last_sec) {
    last_sec = (uint8_t)s;
    if (pl_trk && trk_ended()) { pt3_end = 1; return; }   // the GS looped: the next track at once
    player_draw();
  }
}

uint8_t player_key(uint16_t k)
{
  if (k == K_F1 + 6) { player_pause(); return 1; }
  if (k == (K_F1 + 6 | KM_SHIFT)) { player_stop(); return 1; }
  if (k == K_F1 + 7) { if (pl_state != PL_STOP || pl_count) player_next(1); return 1; }
  if (k == (K_F1 + 7 | KM_SHIFT)) { if (pl_state != PL_STOP || pl_count) player_next(-1); return 1; }
  return 0;
}
