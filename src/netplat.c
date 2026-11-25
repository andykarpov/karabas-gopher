// Time and cancel for the network code (src/net/transport.h), in the common code.
#include "ts.h"
#include "kbd.h"
#include "net/transport.h"

uint16_t net_ticks(void)
{
  return ticks;
}

// polls the keys once a frame: Esc cancels the wait
uint8_t net_idle(void)
{
  static uint16_t last;
  if (ticks == last) return 0;
  last = ticks;
  return kbd_get() == K_ESC;
}
