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
void player_clear(void);
uint8_t player_add(const gurl_t *u, const char *name);
#define PL_NOTPT3 0x20                  // player_play: the file is not a PT3 module
uint8_t player_play(uint8_t i);         // loads and starts track i: a G_* code (G_OK) or PL_NOTPT3
void player_stop(void);
void player_pause(void);                // pause / go on
uint8_t player_next(int8_t d);          // the next (d 1) / previous (d -1) track
// between keys (ui.c): the end of a track -> the next one; the time on the status row
void player_tick(void);
// a key of the player (F7, Shift+F7, F8, Shift+F8): 1 if it was one
uint8_t player_key(uint16_t k);
void player_draw(void);                 // its part of the status row (when the browser shows it)
// a selector that is a module the player plays (.pt3)
uint8_t player_playable(const char *sel, uint8_t slen);

// crt0.s
extern volatile uint8_t pt3_on, pt3_end;
extern uint16_t pt3_mod2;              // the TurboSound half (0: one AY), read by INIT
void pt3_cmd(uint8_t entry);           // 0 INIT, 6 MUTE
