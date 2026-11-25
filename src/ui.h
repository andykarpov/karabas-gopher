// Screen pieces shared by the screens of bank BANK_UI (ui.c is in that bank: plain calls).
#pragma once
#include <stdint.h>
#include "ts.h"
#include "text.h"

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

// clears the screen: the title bar, the help line at the bottom, the layout mark
void ui_frame(const char *title, const char *help);
void ui_layout(void);                  // РУС / LAT at the right of the help line
uint16_t ui_key(void);                 // waits for a key; Ins switches the layout
// edits buf (max characters) in place at x, y, w cells; 1 = Enter, 0 = Esc (buf restored);
// hidden: 0 plain, 1 password shown as * (Tab shows / hides), 2 plain without the Tab toggle
uint8_t ui_edit(uint8_t x, uint8_t y, uint8_t w, char *buf, uint8_t max, uint8_t hidden);
// a question on row y: 1 = Enter, 0 = Esc
uint8_t ui_ask(uint8_t y, const char *q);
// a line with a number: "text 123"
void ui_num(char *out, const char *text, int16_t v);
