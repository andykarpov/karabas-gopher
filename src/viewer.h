// The picture viewer (bank BANK_UI): the pictures of a menu (pages PG_PICLIST), full screen, the
// neighbours by the arrows, S = a slide show (VW_SLIDE s a picture, the next one loads meanwhile).
#pragma once
#include <stdint.h>
#include "gopher/url.h"

#define VW_SLIDE 5
#define VW_PIC   0x30                   // viewer_run: VW_PIC + PICE_* (pic.h)
extern uint8_t vw_count, vw_loading;
void viewer_clear(void);
uint8_t viewer_add(const gurl_t *u, const char *name);
// shows picture i, returns when the user leaves (text mode back); the error of the first load or 0
uint8_t viewer_run(uint8_t i);
