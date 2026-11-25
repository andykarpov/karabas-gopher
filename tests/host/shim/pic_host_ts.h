// host stand-in of src/ts.h for the picture decoders: 256 pages of 16 KB, window 3 = the page chosen
#pragma once
#include <stdint.h>
extern uint8_t host_mem[256][0x4000];
extern uint8_t host_page3, host_cram[512], host_vpage, host_vconf, host_fm;
#define PAGE3(p) (host_page3 = (uint8_t)(p))
#define WIN3 (host_fm ? host_cram : host_mem[host_page3])
#define PG_PICPAL 0x5B
#define PG_PICA   0x60
#define PG_PICB   0x70
#define PG_BIG    0x80
#define FM_EN 0x10
static uint8_t TS_FMADDR_dummy __attribute__((unused));
#define TS_FMADDR host_fm
#define TS_VPAGE host_vpage
#define TS_VCONFIG host_vconf
#define TS_PALSEL TS_FMADDR_dummy
#define TS_BORDER TS_FMADDR_dummy
#define TS_GXOFFSL TS_FMADDR_dummy
#define TS_GXOFFSH TS_FMADDR_dummy
#define TS_GYOFFSL TS_FMADDR_dummy
#define TS_GYOFFSH TS_FMADDR_dummy
#define __critical
