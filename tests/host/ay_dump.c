// The C AY players (src/player/stc.c, asc.c) on the PC: ay_dump MODULE FRAMES prints the registers
// after every frame as "r0 .. r13 w13" in hex (w13 = 1 if register 13 was written), the format of
// the reference dumps in tests/data/ay/*.regs. Built once per player (-include of the player file).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/player/ay.h"

uint8_t *ay_m;
uint8_t ay_r[14], ay_env, ay_flags;

int main(int argc, char **argv)
{
  static uint8_t mod[65536];
  FILE *f;
  int i, r, frames;
  if (argc < 3) { fprintf(stderr, "ay_dump MODULE FRAMES\n"); return 2; }
  f = fopen(argv[1], "rb");
  if (!f) { perror(argv[1]); return 1; }
  if (!fread(mod, 1, sizeof(mod), f)) return 1;
  fclose(f);
  ay_m = mod;
  frames = atoi(argv[2]);
  ay_init();
  for (i = 0; i < frames; i++) {
    ay_env = 0;
    ay_play();
    for (r = 0; r < 14; r++) printf("%02x ", ay_r[r]);
    printf("%d\n", ay_env);
  }
  fprintf(stderr, "flags %02x\n", ay_flags);
  return 0;
}
