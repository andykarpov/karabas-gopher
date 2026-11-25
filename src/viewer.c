#include <string.h>
#include "ts.h"
#include "ui.h"
#include "kbd.h"
#include "player.h"
#include "viewer.h"
#include "pic.h"
#include "gopher/gopher.h"
#include "browser.h"

uint8_t vw_count, vw_loading;
extern gurl_t ui_go;                    // the shared URL of the browser (RAM, goes to bank 3)
#define vu ui_go
static char vname[PL_NAME];

void viewer_clear(void) { vw_count = 0; }

uint8_t viewer_add(const gurl_t *u, const char *name)
{
  if (!mlist_put(PG_PICLIST, vw_count, u, name)) return 0;
  vw_count++;
  return 1;
}

// picture i into buffer b: 0, a G_* code or VW_PIC + PICE_*
static uint8_t load(uint8_t i, uint8_t b)
{
  uint8_t r, f;
  mlist_get(PG_PICLIST, i, &vu, vname);
  f = pic_format(vu.sel, (uint8_t)strlen(vu.sel));
  r = gopher_load_big(&vu);
  if (r) return r;
  r = pic_decode(f, gopher_bytes, b);
  return r ? VW_PIC + r : 0;
}

static uint8_t wrap(uint8_t i, int8_t d)
{
  if (d < 0) return i ? i - 1 : vw_count - 1;
  return i + 1 < vw_count ? i + 1 : 0;
}

uint8_t viewer_run(uint8_t i)
{
  uint8_t r, b = 0, slide = 0, ready = 0, next = 0, n;
  uint16_t k, t0 = 0, left;
  vw_loading = 1;
  r = load(i, 0);
  vw_loading = 0;
  if (r) return r;
  pic_show(0);
  for (;;) {
    if (slide && !ready) {
      // the next picture into the other buffer while this one is shown (failed ones skipped)
      for (n = vw_count, next = i; n; n--) {
        next = wrap(next, 1);
        r = load(next, b ^ 1);
        if (!r) { ready = 1; break; }
        if (r == G_CANCEL) { slide = 0; break; }
      }
      if (!ready) slide = 0;
      continue;
    }
    left = 0xFFFF;
    if (slide) {
      uint16_t e = ticks - t0;
      left = e >= VW_SLIDE * 50 ? 0 : VW_SLIDE * 50 - e;
    }
    k = left ? ui_key_to(left) : K_NONE;
    if (k == K_NONE) {
      if (slide && ready) {
        b ^= 1;
        pic_show(b);
        i = next;
        ready = 0;
        t0 = ticks;
      }
      continue;
    }
    if (slide) { slide = 0; ready = 0; continue; }   // any key stops the slide show
    switch (k) {
      case K_ESC: case K_BS: case K_ENTER:
        txt_mode();
        return 0;
      case K_RIGHT: case K_DOWN: case K_PGDN: case ' ':
      case K_LEFT: case K_UP: case K_PGUP:
        for (n = vw_count, next = i; n; n--) {
          next = wrap(next, k == K_LEFT || k == K_UP || k == K_PGUP ? -1 : 1);
          r = load(next, b ^ 1);
          if (!r) { b ^= 1; pic_show(b); i = next; break; }
          if (r == G_CANCEL) break;
        }
        break;
      case 's': case 'S': case 0xEB: case 0x9B:        // S, also Ы of the Russian layout
        if (vw_count > 1) { slide = 1; ready = 0; t0 = ticks; }
        break;
    }
  }
}
