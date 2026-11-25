// Screen pieces shared by the screens of bank BANK_UI (ui.c is in that bank: plain calls).
#pragma once
#include <stdint.h>
#include "ts.h"
#include "text.h"
#include "bank.h"

#define A_BG     ATTR(COL_WHITE, COL_BLUE)
#define A_TITLE  ATTR(COL_BYELLOW, COL_CYAN)
#define A_SEL    ATTR(COL_BLACK, COL_CYAN)
#define A_EDIT   ATTR(COL_BWHITE, COL_BLACK)
#define A_CUR    ATTR(COL_BLACK, COL_BWHITE)
#define A_HELP   ATTR(COL_BLACK, COL_CYAN)
#define A_MSG    ATTR(COL_BYELLOW, COL_BLUE)
#define A_OK     ATTR(COL_BGREEN, COL_BLUE)
#define A_DIM    ATTR(COL_GREY, COL_BLUE)

#define LX       2          // left margin

// own glyphs (ui_glyphs): signal = G_SIG_L + 0..2, G_SIG_R + 0..2; the font has ♪ #0D and ▶ #10
#define G_SIG_L  0x01
#define G_SIG_R  0x04
#define G_LOCK   0x0B
#define G_PAUSE  0x0C
#define G_NOTE   0x0D
#define G_PLAY   0x10

// the header at the right of row 0: " ssid(16) SS L HH:MM "; titles end before HDR_X
#define HDR_SSID 16
#define HDR_X    (TXT_COLS - 1 - 5 - 1 - 1 - 1 - 2 - 1 - HDR_SSID - 1)

void ui_glyphs(void) BANKED;           // once at start: the glyphs into the font
void ui_title(const char *title);      // row 0: the title + the header
void ui_header(void);                  // redraws the network + time part of row 0
void ui_poll_soon(void);               // poll the link at the next ui_key wait
extern uint8_t ui_lay_y;               // the row of the РУС / LAT mark (ui_frame: the last row)
// s at x, y in w cells, attribute a; text between ~ ~ (key names) in ak
void ui_hint(uint8_t x, uint8_t y, uint8_t w, uint8_t a, uint8_t ak, const char *s);

// clears the screen: the title bar, the help line at the bottom, the layout mark
void ui_frame(const char *title, const char *help);
void ui_layout(void);                  // РУС / LAT at the right of the help line
uint16_t ui_key(void);                 // waits for a key; Ins switches the layout
uint16_t ui_key_to(uint16_t frames);   // the same, K_NONE after `frames` frames
// edits buf (max characters) in place at x, y, w cells; 1 = Enter, 0 = Esc (buf restored);
// hidden: 0 plain, 1 password shown as * (Tab shows / hides), 2 plain without the Tab toggle
uint8_t ui_edit(uint8_t x, uint8_t y, uint8_t w, char *buf, uint8_t max, uint8_t hidden);
// a question on row y: 1 = Enter, 0 = Esc
uint8_t ui_ask(uint8_t y, const char *q);
// a line with a number: "text 123"
void ui_num(char *out, const char *text, int16_t v);
