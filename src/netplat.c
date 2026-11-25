// Time and cancel for the network code (src/net/transport.h), in the common code.
#include "ts.h"
#include "kbd.h"
#include "net/transport.h"

uint16_t net_ticks(void)
{
  return ticks;
}

uint8_t net_bg;

// polls the keys once a frame: Esc cancels the wait (not in a background poll: the keys stay queued)
uint8_t net_idle(void)
{
  static uint16_t last;
  if (net_bg || ticks == last) return 0;
  last = ticks;
  return kbd_get() == K_ESC;
}
