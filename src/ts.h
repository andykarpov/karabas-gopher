// TS-Conf registers (ports #nnAF, nn = register) and the memory map of the browser.
// Register numbers and bits after zx-evo pentevo/docs/TSconf and the tsconf RTL.
#pragma once
#include <stdint.h>

__sfr __banked __at(0x00AF) TS_VCONFIG;
__sfr __banked __at(0x01AF) TS_VPAGE;
__sfr __banked __at(0x06AF) TS_TSCONFIG;
__sfr __banked __at(0x07AF) TS_PALSEL;
__sfr __banked __at(0x0FAF) TS_BORDER;
__sfr __banked __at(0x10AF) TS_PAGE0;
__sfr __banked __at(0x11AF) TS_PAGE1;
__sfr __banked __at(0x12AF) TS_PAGE2;
__sfr __banked __at(0x13AF) TS_PAGE3;
__sfr __banked __at(0x15AF) TS_FMADDR;
__sfr __banked __at(0x20AF) TS_SYSCONFIG;
__sfr __banked __at(0x21AF) TS_MEMCONFIG;
__sfr __banked __at(0x2AAF) TS_INTMASK;

// VConfig
#define VID_320X240   0x80
#define VID_TEXT      0x03
#define VID_NOGFX     0x20
// FMAddr: bits 3..0 = A15..A12 of the 4 KB window, bit 4 = enable; CRAM at +#000 (256 x RGB555)
#define FM_EN         0x10
#define FM_CRAM       0x0000

// Memory map (crt0.s, bank.h): window 0 = common code + data + stack (page #20), windows 1 + 2 = the
// current bank (pages #30 + 2n, #31 + 2n), window 3 = paged buffers.
#define PG_COMMON     0x20
#define PG_SCRATCH    0x23
#define PG_BANK(n)    (0x30 + 2 * (n))
#define PG_TEXT       0x24    // text mode: chars + attributes (VPAGE)
#define PG_FONT       0x25    // its font: VPAGE ^ 1, 2 KB at the start
#define WIN3          ((uint8_t *)0xC000)

// text mode 320x240: 80 x 30 characters of 8x8 (half-width pixels), a line is 256 bytes:
// codes at +0..+127, attributes at +128..+255 (low nibble = ink, high nibble = paper)
#define TXT_COLS      80
#define TXT_ROWS      30
#define TXT_LINE      256
#define TXT_ATTR      128

extern volatile uint16_t ticks;   // frames since start (IM 1 handler of crt0.s)
