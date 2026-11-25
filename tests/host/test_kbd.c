#include "check.h"
#include "../../src/kbd.h"

static kbd_state_t s;

// feeds the bytes, returns the last key
static unsigned keys(const unsigned char *c, int n)
{
  unsigned k = 0, r;
  for (int i = 0; i < n; i++) if ((r = kbd_decode(&s, c[i]))) k = r;
  return k;
}
#define K(...) keys((const unsigned char[]){__VA_ARGS__}, sizeof((const unsigned char[]){__VA_ARGS__}))

int main(void)
{
  kbd_reset(&s);
  CHECK_EQ(K(0x1C), 'a');
  CHECK_EQ(K(0xF0, 0x1C), 0);                 // release: no key
  CHECK_EQ(K(0x12, 0x1C, 0xF0, 0x1C, 0xF0, 0x12), 'A');
  CHECK_EQ(K(0x1C), 'a');                     // shift released
  CHECK_EQ(K(0x12, 0x16, 0xF0, 0x12), '!');
  CHECK_EQ(K(0x59, 0x52, 0xF0, 0x59), '"');   // right shift
  CHECK_EQ(K(0x58, 0xF0, 0x58, 0x1C), 'A');   // caps lock
  CHECK_EQ(K(0x16), '1');                     // caps does not shift digits
  CHECK_EQ(K(0x58, 0xF0, 0x58), 0);
  CHECK_EQ(K(0xE0, 0x75), K_UP);
  CHECK_EQ(K(0xE0, 0xF0, 0x75), 0);
  CHECK_EQ(K(0x75), K_UP);                    // keypad 8, Num Lock off
  CHECK_EQ(K(0xE0, 0x71), K_DEL);
  CHECK_EQ(K(0xE0, 0x70), K_INS);
  CHECK_EQ(K(0x5A), K_ENTER);
  CHECK_EQ(K(0xE0, 0x5A), K_ENTER);
  CHECK_EQ(K(0x66), K_BS);
  CHECK_EQ(K(0x76), K_ESC);
  CHECK_EQ(K(0x05), K_F1);
  CHECK_EQ(K(0x07), K_F1 + 11);
  CHECK_EQ(K(0x83), K_F1 + 6);
  CHECK_EQ(K(0x14, 0x35, 0xF0, 0x14), 'y' | KM_CTRL);
  CHECK_EQ(K(0xE0, 0x14, 0xE0, 0x71, 0xE0, 0xF0, 0x14), K_DEL | KM_CTRL);   // right ctrl
  CHECK_EQ(K(0x12, 0xE0, 0x6C, 0xF0, 0x12), K_HOME | KM_SHIFT);
  CHECK_EQ(K(0xE0, 0x12, 0xE0, 0x7C), 0);     // Print Screen: no key, no stuck shift
  CHECK_EQ(K(0x1C), 'a');
  CHECK_EQ(K(0xFF), 0);
  // Russian
  s.rus = 1;
  CHECK_EQ(K(0x15), 0xA9);                    // q -> й
  CHECK_EQ(K(0x12, 0x15, 0xF0, 0x12), 0x89);  // Й
  CHECK_EQ(K(0x0E), 0xF1);                    // ё
  CHECK_EQ(K(0x4A), '.');
  CHECK_EQ(K(0x12, 0x4A, 0xF0, 0x12), ',');
  CHECK_EQ(K(0x58, 0xF0, 0x58, 0x1C), 0x94);  // caps: Ф
  CHECK_EQ(K(0x4A), '.');                     // caps does not change punctuation
  CHECK_EQ(K(0x16), '1');
  CHECK_EQ(K(0x14, 0x15, 0xF0, 0x14), 'q' | KM_CTRL);   // Ctrl combos stay Latin
  DONE();
}
