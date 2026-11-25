#pragma once
#include <stdint.h>

enum { COL_BLACK, COL_BLUE, COL_RED, COL_MAGENTA, COL_GREEN, COL_CYAN, COL_YELLOW, COL_WHITE,
       COL_GREY, COL_BBLUE, COL_BRED, COL_BMAGENTA, COL_BGREEN, COL_BCYAN, COL_BYELLOW, COL_BWHITE };
#define ATTR(ink, paper) (uint8_t)(((paper) << 4) | (ink))

void txt_init(void);
void txt_clear(uint8_t attr);
void txt_goto(uint8_t x, uint8_t y);
void txt_attr(uint8_t attr);
void txt_putc(char c);
void txt_puts(const char *s);
