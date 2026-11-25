// The common part of the AY player blobs written in C (stc.c, asc.c): each is linked on its own at
// #4000 of page PG_MUSIC with aycrt.s (the entry table of pt3.asm) and plays the module at PT3_MOD
// (#5200) from the frame interrupt. The players keep a shadow of the AY registers; aycrt.s writes
// it out after every frame (register 13 only when the player set ay_env: writing it restarts the
// envelope). Built on the PC too (-DHOST, tests/host/test_ay.c) to compare frames with references.
#pragma once
#include <stdint.h>

#ifdef HOST
extern uint8_t *ay_m;
#define M(o)  ay_m[(uint16_t)(o)]
#else
#define M(o)  (*(uint8_t *)(uint16_t)(0x5200 + (uint16_t)(o)))   // PT3_MOD in src/ts.h
#endif
#define W(o)  ((uint16_t)(M(o) | (uint16_t)M((uint16_t)(o) + 1) << 8))

enum { R_TONA, R_TONA_H, R_TONB, R_TONB_H, R_TONC, R_TONC_H, R_NOISE, R_MIXER,
       R_VOLA, R_VOLB, R_VOLC, R_ENV, R_ENV_H, R_SHAPE };

extern uint8_t ay_r[14];       // the registers of this frame
extern uint8_t ay_env;         // 1: register 13 was set this frame
extern uint8_t ay_flags;       // bit 7: the song came back to its loop point (aycrt.s: +9)

void ay_init(void);            // the module at M(0): from the start
void ay_play(void);            // one frame into ay_r
