// TS-Conf registers (ports #nnAF, nn = register) and the memory map of the browser.
// Register numbers and bits after zx-evo pentevo/docs/TSconf and the tsconf RTL.
#pragma once
#include <stdint.h>

__sfr __banked __at(0x00AF) TS_VCONFIG;
__sfr __banked __at(0x01AF) TS_VPAGE;
__sfr __banked __at(0x02AF) TS_GXOFFSL;
__sfr __banked __at(0x03AF) TS_GXOFFSH;
__sfr __banked __at(0x04AF) TS_GYOFFSL;
__sfr __banked __at(0x05AF) TS_GYOFFSH;
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
#define PG_SCRATCH    0x23    // the INI text of config.c (2 KB at #C000)
#define PG_BANK(n)    (0x30 + 2 * (n))   // banks 1..7: #32..#3F
#define PG_DOC        0x40    // 16 pages: the document (gopher/doc.c)
#define PG_IDX        0x50    // 2 pages: its row index
#define PG_HIST       0x52    // the history (browser.c)
#define PG_NETBUF     0x53    // the AT receive ring / ZiFi32 chunks (1 KB)
#define PG_MUSIC      0x54    // 3 pages: an AY player at #4000 (from PG_PLAYERS) + the module at PT3_MOD
#define PG_LIST       0x57    // 2 pages: the play list (player.c)
#define PG_PICLIST    0x59    // 2 pages: the pictures of a menu (viewer.c)
#define PG_PICPAL     0x5B    // the palettes of the two picture buffers (512 bytes each)
#define PG_PLAYERS    0x5C    // the AY player blobs (SPG; tools/mkplayers.py), one copied into PG_MUSIC
#define PG_PICA       0x60    // 16 pages: picture buffer A (16c: VPAGE multiple of 8, 256c: of 16)
#define PG_PICB       0x70    // 16 pages: picture buffer B (the next one in a slide show)
#define PG_BIG        0x80    // 128 pages (2 MB): a tracker module on its way to the GS / ZiFi32
#define BIG_MAX       0x200000UL
#define PT3_MOD       0x5200  // the module in windows 1..3 while the player runs (2 players before it)
#define PT3_MAX       ((uint16_t)(0x10000L - PT3_MOD))   // to the end of window 3: 44544
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

// Window 3: always through PAGE3 (the shadow first): the interrupt maps the music pages into
// windows 1..3 for the player and puts back page3 and the bank (crt0.s)
extern volatile uint8_t page3;
#define PAGE3(p) do { page3 = (p); TS_PAGE3 = page3; } while (0)
