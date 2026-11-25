// bank BANK_UI
#pragma once
#include <stdint.h>
#include "bank.h"
// m: RAM or common code (bank.h); code < 100 is added as " (FatFs NN)"
void settings_msg(const char *m, uint8_t code) BANKED;
void settings_run(void) BANKED;
