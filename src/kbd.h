// Keyboard: PS/2 set 2 scan codes (the key buffer of the core, CMOS registers #F0..#FF) -> keys.
// The decoder (kbd_decode) is plain C, tested on the host; kbd_poll reads the hardware (kbd_hw.c).
#pragma once
#include <stdint.h>

// key codes: 0x20..0xFF = a CP866 character; below 0x20 and from 0x100 = special keys;
// KM_* bits are added to special keys and to Ctrl / Alt combinations
#define K_NONE    0x0000
#define K_BS      0x0008
#define K_TAB     0x0009
#define K_ENTER   0x000D
#define K_ESC     0x001B
#define K_UP      0x0101
#define K_DOWN    0x0102
#define K_LEFT    0x0103
#define K_RIGHT   0x0104
#define K_HOME    0x0105
#define K_END     0x0106
#define K_PGUP    0x0107
#define K_PGDN    0x0108
#define K_INS     0x0109
#define K_DEL     0x010A
#define K_F1      0x0111    // .. K_F1 + 11 = F12
#define KM_SHIFT  0x1000
#define KM_CTRL   0x2000
#define KM_ALT    0x4000
#define K_CODE(k) ((k) & 0x0FFF)

typedef struct {
  uint8_t e0, brk;          // prefix state of the current scan code
  uint8_t lshift, rshift, ctrl, alt, caps;
  uint8_t rus;              // 1: Russian layout (JCUKEN), toggled by the caller (Ins in the browser)
} kbd_state_t;

void kbd_reset(kbd_state_t *s);
// one scan code byte in; a key out (K_NONE while a code is incomplete, for releases and modifiers)
uint16_t kbd_decode(kbd_state_t *s, uint8_t code);

// hardware (kbd_hw.c)
extern kbd_state_t kbd;
extern uint8_t kbd_ok;      // 0: no PS/2 key buffer found
void kbd_init(void);
uint16_t kbd_get(void);     // next key or K_NONE, never waits
