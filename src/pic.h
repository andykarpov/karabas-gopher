// Pictures (bank BANK_PIC): a file of the big buffer (PG_BIG..) is decoded into picture buffer
// 0 (PG_PICA) or 1 (PG_PICB) with its palette (PG_PICPAL), pic_show puts it on the screen.
// Formats as the Wild Commander viewers: .scr (6912 / 6144, ZX mode), .mc / .mlt (6144 pixels +
// 6144 attributes of 8x1 cells, lines in order, as MCGSV of WC: -> 16c), .img (two screens of 6912,
// gigascreen: -> 256c with the mixed colours, no flicker), .bmp (4 / 8 bits, up to 360x288: 16c / 256c),
// .gif (the first image, any size: scaled down by a whole factor to 360x288, 256c).
#pragma once
#include <stdint.h>
#include "bank.h"

enum { PICF_NONE, PICF_SCR, PICF_MC, PICF_IMG, PICF_BMP, PICF_GIF };
// pic_decode: wrong size, a BMP it cannot show, a broken / unsupported GIF
enum { PICE_OK, PICE_SIZE, PICE_BMP, PICE_GIF };

uint8_t pic_format(const char *sel, uint8_t slen) BANKED;      // by the extension
uint8_t pic_decode(uint8_t fmt, uint32_t len, uint8_t buf) BANKED;
void pic_show(uint8_t buf) BANKED;          // after a frame: its video mode, page and palette
