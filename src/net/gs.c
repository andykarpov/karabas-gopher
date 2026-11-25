#include <string.h>
#include "ts.h"
#include "gs.h"
#include "transport.h"

__sfr __at(0xBB) GS_CMD;
__sfr __at(0xB3) GS_DATA;

#define ST_DATA 0x80              // status bit 7: a data byte waits (for the GS or for us)
#define ST_CMD  0x01              // status bit 0: a command not taken yet

static uint8_t gs_handle;
static void volume(uint8_t v);

// a command; waits until the GS took it (bit 0 clear), at most 1 s
static uint8_t cmd(uint8_t c)
{
  uint16_t t0 = net_ticks();
  GS_CMD = c;
  while (GS_CMD & ST_CMD)
    if ((uint16_t)(net_ticks() - t0) > 50) return 1;
  return 0;
}

uint8_t gs_detect(void)
{
  uint16_t t0;
  if (GS_CMD == 0xFF) return 0;            // nothing on the port
  GS_CMD = 0xF3;                           // warm reset: the GS comes back in well under a second
  t0 = net_ticks();
  while (GS_CMD & ST_CMD)
    if ((uint16_t)(net_ticks() - t0) > 100) return 0;
  return !cmd(0x36) && GS_DATA == 0xFF;    // #36: data = #FF
}

uint8_t gs_play_module(uint32_t len)
{
  uint32_t off = 0;
  if (cmd(0xF3) || cmd(0x30)) return 1;    // a fresh GS, then "load module": its handle
  gs_handle = GS_DATA;
  if (cmd(0xD1)) return 1;                 // open the stream
  while (off < len) {
    uint16_t o = (uint16_t)off & 0x3FFF, k = 0x4000 - o;
    if (k > len - off) k = (uint16_t)(len - off);
    PAGE3(PG_BIG + (uint8_t)(off >> 14));
    if (gs_put(WIN3 + o, k)) return 1;
    off += k;
  }
  if (cmd(0xD2)) return 1;                 // close: the GS reads the module
  volume(0x40);
  GS_DATA = gs_handle;
  return cmd(0x31);                        // play
}

// the module volume (#2A, 0..#40); at 0 the GS mixes silence, so a stop leaves the DAC at its middle
// and not at the last sample (a step = a click on pause / stop)
static void volume(uint8_t v)
{
  GS_DATA = v;
  cmd(0x2A);
}

void gs_stop(void)
{
  uint16_t t0;
  volume(0);
  t0 = net_ticks();
  while ((uint16_t)(net_ticks() - t0) < 3) ;   // a few frames of silence first
  cmd(0x32);
}

void gs_continue(void)
{
  cmd(0x33);
  volume(0x40);
}

uint8_t gs_song_pos(void)
{
  if (cmd(0x60)) return 0xFF;
  return GS_DATA;
}
