// General Sound of the core (ports #BB command / status, #B3 data; GS ROM 1.05a, 2 MB). Bank BANK_NET.
#pragma once
#include <stdint.h>

uint8_t gs_detect(void);                   // 1 if a GS answers
// uploads the module of the big buffer (PG_BIG.., len bytes) and plays it: 0 ok
uint8_t gs_play_module(uint32_t len);
void gs_stop(void);                        // #32
void gs_continue(void);                    // #33
uint8_t gs_song_pos(void);                 // #60, 0xFF if no answer
// common code (crt0.s): n bytes to #B3, each after the GS took the last one; 0 ok, 1 timeout
uint8_t gs_put(const uint8_t *p, uint16_t n);
