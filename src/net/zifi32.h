// ZiFi32: the ESP32-S3 of tslabs on the SPI of the Z-controller (pentevo/esp32, our fork
// andykarpov/zx-evo karabas/esp32-fixes; protocol: pentevo/esp32/doc/esp32-spi-wifi.md of PR #95).
// Bank BANK_NET. Works on the firmware already flashed: every wait has a timeout (the upstream
// firmware can stay BUSY after a gopher / stream command until a RESET, tslabs/zx-evo#94).
#pragma once
#include <stdint.h>
#include "esp_at_parse.h"

enum { ZF_OK, ZF_ERROR, ZF_TIMEOUT, ZF_CANCEL, ZF_NOSEL };

typedef struct {
  uint8_t api;
  uint16_t feat;
  uint8_t ver[3];
  char build[24];          // GET_INFO_STR 2
} zf_info_t;

uint8_t zf_detect(zf_info_t *info);                  // 1 if a ZiFi32 answers GET_VER
uint8_t zf_reset(void);                              // RESET (#EE), waits for it to come back
uint8_t zf_netstate(void);                           // 0 closed, 1 opening, 2 open, 3 closing, FF unknown
uint8_t zf_own_ip(char *ip);                         // "a.b.c.d" or ""
uint8_t zf_scan(kg_wifi_ap_t *aps, uint8_t *count, uint8_t max);
uint8_t zf_join(const char *ssid, const char *pass); // ZF_OK when the network is up
uint8_t zf_leave(void);
uint8_t zf_last_status(void);                        // the STATUS of the last failed command

// gopher stream: the URL is built from host / port / selector (see zifi32.c on the selector)
uint8_t zf_gopher_open(const char *host, uint16_t port, const char *sel);
// >0, -1 the end, -2 no answer / error, -3 cancelled (Esc: the current chunk is finished first)
int16_t zf_gopher_read(uint8_t *buf, uint16_t max);
uint8_t zf_gopher_close(void);                     // 1: the module was stuck and has been reset

// tracker modules (firmware with FEAT >= 10: the July 2026 command codes); the module is taken from
// the big buffer PG_BIG..; one at a time, its object deleted by zf_track_free
enum { ZF_XM = 0x05, ZF_MOD = 0x0A, ZF_S3M = 0x0C };
#define zf_track_ok(info) ((info)->feat >= 10)
uint8_t zf_track_play(uint8_t type, uint32_t len);
uint8_t zf_track_pause(uint8_t on);       // 1: TRACK_STOP, 0: TRACK_PLAY again
void zf_track_free(void);
