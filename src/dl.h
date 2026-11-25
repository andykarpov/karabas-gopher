// Downloads to the SD card (bank BANK_FS, where FatFs is). All pointers: RAM (bank.h).
#pragma once
#include <stdint.h>
#include "bank.h"

#define DL_FULL 0xFE        // dl_write: the card is full

// creates path ("dir/.../name"; missing folders are made); a taken name gets "_1".."_99" before its
// extension, written back into path (room = the size of its buffer). FatFs FRESULT, 0 = ok
uint8_t dl_open(char *path, uint8_t room) BANKED;
uint8_t dl_write(const uint8_t *buf, uint16_t n) BANKED;
// keep 0: the file is deleted (cancel / error); cut: bytes dropped from its end (a gopher ".")
uint8_t dl_close(uint8_t keep, uint8_t cut) BANKED;
