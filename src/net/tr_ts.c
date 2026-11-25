// The TS ZiFi API v1 of the core (karabas-go-core-tsconf src/rtl/uart/zifi.vhd): reading #C0EF / #C1EF
// selects the ESP8266 UART, #C2EF / #C3EF the RS232 to the MCU (USB serial); #BFEF is the data
// port of the selected one (#xxEF with xx <= #BF), #C7EF the command (1 / 2 / 3 clear rx / tx /
// both, #FF: then #C7EF reads 1 if the API is there). #C0EF / #C2EF = received bytes (191 max),
// #C1EF / #C3EF = free bytes to send. The core holds the ESP with CTS above 1792 received bytes.
#include "transport.h"
#include "tr_ts.h"

__sfr __banked __at(0xC0EF) ZF_RX;
__sfr __banked __at(0xC1EF) ZF_TX;
__sfr __banked __at(0xC2EF) RS_RX;
__sfr __banked __at(0xC3EF) RS_TX;
__sfr __banked __at(0xC7EF) ZF_CMD;
__sfr __banked __at(0xBFEF) ZF_DATA;

uint8_t ts_zifi_api(void)
{
  ZF_CMD = 0xFF;
  return ZF_CMD == 0x01;
}

static uint8_t z_avail(void) { return ZF_RX; }
static uint8_t z_room(void) { return ZF_TX; }
static void z_read(uint8_t *b, uint8_t n) { (void)ZF_RX; while (n--) *b++ = ZF_DATA; }
static void z_write(const uint8_t *b, uint8_t n) { (void)ZF_TX; while (n--) ZF_DATA = *b++; }
static void z_clear(void) { (void)ZF_RX; ZF_CMD = 0x03; }

static uint8_t r_avail(void) { return RS_RX; }
static uint8_t r_room(void) { return RS_TX; }
static void r_read(uint8_t *b, uint8_t n) { (void)RS_RX; while (n--) *b++ = ZF_DATA; }
static void r_write(const uint8_t *b, uint8_t n) { (void)RS_TX; while (n--) ZF_DATA = *b++; }
static void r_clear(void) { (void)RS_RX; ZF_CMD = 0x03; }

const transport_t tr_zifi = { z_avail, z_read, z_room, z_write, z_clear };
const transport_t tr_rs232 = { r_avail, r_read, r_room, r_write, r_clear };
