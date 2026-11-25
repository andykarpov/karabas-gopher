// A byte pipe to an AT modem: the ZiFi / RS232 FIFOs of the core (tr_ts.c), a tty on the host.
#pragma once
#include <stdint.h>

typedef struct {
  uint8_t (*avail)(void);                        // bytes that can be read now (at most 191)
  void (*read)(uint8_t *buf, uint8_t n);         // n <= avail()
  uint8_t (*room)(void);                         // bytes that can be written now
  void (*write)(const uint8_t *buf, uint8_t n);  // n <= room()
  void (*clear)(void);                           // drop what is in the FIFOs
} transport_t;

// time and cancel of the platform (net_ts.c, the host tests)
uint16_t net_ticks(void);    // 50 a second
uint8_t net_idle(void);      // between polls of a wait: 1 = the user cancelled (Esc)
