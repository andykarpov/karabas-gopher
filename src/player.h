// The background music player (bank BANK_UI): a play list of the music links of one menu (pages
// PG_LIST), PT3 modules loaded into the music pages and played by the frame interrupt (crt0.s).
#pragma once
#include <stdint.h>
#include "bank.h"
#include "gopher/url.h"

enum { PL_STOP, PL_PLAY, PL_PAUSE };
extern uint8_t pl_state, pl_count, pl_cur;

// the play list: player_clear, then player_add for each track (RAM strings); 0 if it is full
#define PL_HOST  48
#define PL_SEL   160
#define PL_NAME  44
#define PL_MAX   128
// a list of links in 2 pages from pg (256-byte entries); RAM pointers
uint8_t mlist_put(uint8_t pg, uint8_t i, const gurl_t *u, const char *name);
void mlist_get(uint8_t pg, uint8_t i, gurl_t *u, char *name);
void player_clear(void);
uint8_t player_add(const gurl_t *u, const char *name);
#define PL_NOTPT3 0x20                  // player_play: the file is not a PT3 module
#define PL_NOENG  0x21                  // nothing here plays this format (XM / S3M without ZiFi32)
#define PL_ENGINE 0x22                  // the GS / ZiFi32 did not take the module
uint8_t player_play(uint8_t i);         // loads and starts track i: a G_* code (G_OK) or PL_NOTPT3
void player_stop(void);
void player_pause(void);                // pause / go on
uint8_t player_next(int8_t d);          // the next (d 1) / previous (d -1) track
// between keys (ui.c): the end of a track -> the next one; the time on the status row
void player_tick(void);
// a key of the player (F7, Shift+F7, F8, Shift+F8): 1 if it was one
uint8_t player_key(uint16_t k);
void player_draw(void);                 // its part of the status row (when the browser shows it)
// the music format of a selector by its extension: PF_NONE, PF_PT3, PF_MOD, PF_XM, PF_S3M
enum { PF_NONE, PF_PT3, PF_MOD, PF_XM, PF_S3M };
uint8_t player_format(const char *sel, uint8_t slen);
// 1 if this machine plays it (PT3 always; MOD with a GS or ZiFi32; XM / S3M with ZiFi32 0.6.4+)
uint8_t player_can(uint8_t fmt);

// crt0.s
extern volatile uint8_t pt3_on, pt3_end;
extern uint16_t pt3_mod2;              // the TurboSound half (0: one AY), read by INIT
void pt3_cmd(uint8_t entry);           // 0 INIT, 6 MUTE
