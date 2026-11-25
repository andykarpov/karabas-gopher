// Settings of the browser: /zifi/gopher.ini on the SD card (docs/PLAN.md 3).
#pragma once
#include <stdint.h>
#include "bank.h"

#define CFG_DIR   "/zifi"
#define CFG_PATH  "/zifi/gopher.ini"

enum { BK_AUTO, BK_ZIFI32, BK_ZIFI, BK_USB, BK_COUNT };
enum { CS_UTF8, CS_CP1251, CS_KOI8, CS_CP866, CS_COUNT };

typedef struct {
  uint8_t backend;
  uint8_t charset;
  uint8_t autoconn;      // ZiFi32: join the saved network on start
  char home[256];
  char downloads[64];
  char ssid[33];
  char pass[64];
} config_t;

extern config_t cfg;
// in the common code (names.c): both banks use them
extern const char *const backend_names[BK_COUNT];
extern const char *const charset_names[CS_COUNT];
extern const char *const yes_no[2];

// bank BANK_FS
void cfg_defaults(void) BANKED;
uint8_t cfg_mount(void) BANKED;    // FR_OK or a FatFs error
// FR_OK (0) or a FatFs error; a missing file is FR_NO_FILE (the defaults stay)
uint8_t cfg_load(void) BANKED;
uint8_t cfg_save(void) BANKED;
