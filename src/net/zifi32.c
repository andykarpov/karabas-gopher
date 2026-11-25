#include <string.h>
#include "ts.h"
#include "zifi32.h"
#include "transport.h"
#include "at.h"

__sfr __at(0x57) SPI_DATA;
__sfr __at(0x77) SPI_CTRL;

#define CS_ON   0x13           // bit 4: ESP; bit 0 stays 1 (0 hands the bus to the ESP), bit 1: SD off
#define CS_OFF  0x03

#define C_WR_REGS 0x01
#define C_RD_REGS 0x02
#define C_WR_DATA 0x03
#define C_RD_DATA 0x04
#define C_W_END   0x07
#define C_R_END   0x08

#define R_COMMAND 0x00
#define R_STATUS  0x01
#define R_STR     0x02         // STRING_TYPE / STRING_SIZE, API, NETSTATE
#define R_STRDATA 0x03         // STRING_DATA, FEAT
#define R_VER     0x05
#define R_SIZE    0x03         // DATA_SIZE (4)
#define R_OWN_IP  0x30
#define R_OBJTYPE 0x02
#define R_OFFSET  0x07         // DATA_OFFSET (4)
#define R_HANDLE  0x0B

#define S_IDLE    0x00
#define S_READY   0x01
#define S_BUSY    0x02
#define S_M2S     0x03
#define S_S2M     0x04
#define S_RESET   0x7E

#define CMD_GET_INFO_STR 0x01
#define CMD_GET_VER      0x02
#define CMD_NETSTATE     0x11
#define CMD_WSCAN        0x12
#define CMD_AP_NAME      0x13
#define CMD_AP_PWD       0x14
#define CMD_AP_CONNECT   0x16
#define CMD_AP_DISC      0x17
#define CMD_SET_URL      0x18
#define CMD_GET_IP       0x19
#define CMD_G_STREAM     0x25
#define CMD_S_READ       0x26
#define CMD_S_CLOSE      0x27
#define CMD_RESET        0xEE
#define CMD_TRACK_INIT   0xA0  // July 2026 numbering (FEAT 10, 0.6.4): zf_track_ok()
#define CMD_TRACK_PLAY   0xA1
#define CMD_TRACK_STOP   0xA2
#define CMD_MAKE_OBJECT  0xE0
#define CMD_WRITE_OBJECT 0xE1
#define CMD_DELETE_OBJ   0xE3

// the chunk / URL buffer: the receive ring of the AT driver (not used while ZiFi32 is the backend)
static uint16_t chunk_len, chunk_pos;
static uint8_t at_end, last_status;
// Esc while a STREAM_READ runs: the chunk is finished first (soft wait), the module stays in step;
// broken = it did not finish in 3 s: zf_gopher_close resets it
static uint8_t soft, stop_req, broken;

static uint8_t rd8(uint8_t reg)
{
  uint8_t v;
  SPI_CTRL = CS_ON;
  SPI_DATA = C_RD_REGS;
  SPI_DATA = reg;
  (void)SPI_DATA;
  (void)SPI_DATA;
  v = SPI_DATA;
  SPI_CTRL = CS_OFF;
  return v;
}

static void rd(uint8_t reg, uint8_t *p, uint8_t n)
{
  SPI_CTRL = CS_ON;
  SPI_DATA = C_RD_REGS;
  SPI_DATA = reg;
  (void)SPI_DATA;
  (void)SPI_DATA;
  while (n--) *p++ = SPI_DATA;
  SPI_CTRL = CS_OFF;
}

static void wr(uint8_t reg, const uint8_t *p, uint8_t n)
{
  SPI_CTRL = CS_ON;
  SPI_DATA = C_WR_REGS;
  SPI_DATA = reg;
  (void)SPI_DATA;
  while (n--) SPI_DATA = *p++;
  SPI_CTRL = CS_OFF;
}

static void cmd(uint8_t c)
{
  uint8_t b[2];
  b[0] = c;
  b[1] = S_IDLE;
  wr(R_COMMAND, b, 2);
}

static uint32_t rd32(uint8_t reg)
{
  uint8_t b[4];
  rd(reg, b, 4);
  return b[0] | (uint16_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
}

static void end_xfer(uint8_t c)
{
  SPI_CTRL = CS_ON;
  SPI_DATA = c;
  (void)SPI_DATA;
  SPI_CTRL = CS_OFF;
}

// waits for a status that is neither IDLE nor BUSY (nor `also`): it, or ZF_TIMEOUT / ZF_CANCEL as 0xF0+
#define W_TIMEOUT 0xF0
#define W_CANCEL  0xF1
static uint8_t wait(uint16_t timeout, uint8_t also)
{
  uint16_t t0 = net_ticks();
  uint8_t s;
  for (;;) {
    s = rd8(R_STATUS);
    if (s != S_IDLE && s != S_BUSY && s != also) return s;
    if ((uint16_t)(net_ticks() - t0) > timeout) return W_TIMEOUT;
    if (net_idle()) {
      if (!soft) return W_CANCEL;
      stop_req = 1;
      t0 = net_ticks();
      timeout = 50 * 3;
    }
  }
}

static uint8_t result(uint8_t s)
{
  last_status = s;
  if (s == S_READY) return ZF_OK;
  if (s == W_TIMEOUT) return ZF_TIMEOUT;
  if (s == W_CANCEL) return ZF_CANCEL;
  return ZF_ERROR;
}

static uint8_t run(uint8_t c, uint16_t timeout)
{
  cmd(c);
  return result(wait(timeout, 0xFF));
}

uint8_t zf_last_status(void) { return last_status; }

uint8_t zf_detect(zf_info_t *info)
{
  uint8_t s, b[6];
  memset(info, 0, sizeof(*info));
  s = rd8(R_STATUS);
  if (s == 0xFF) return 0;               // nothing drives MISO: no module (Go, Mini)
  if (s == S_BUSY) {                     // a command left running, or still starting
    if (wait(50, 0xFF) >= W_TIMEOUT) return 0;
  }
  if (run(CMD_GET_VER, 50)) return 0;
  rd(R_STR, b, 6);
  info->api = b[0];
  info->feat = b[1] | (uint16_t)b[2] << 8;
  info->ver[0] = b[3]; info->ver[1] = b[4]; info->ver[2] = b[5];
  b[0] = 2;                              // the build string
  wr(R_STR, b, 1);
  if (!run(CMD_GET_INFO_STR, 50)) {
    uint8_t n = rd8(R_STR);
    if (n > sizeof(info->build) - 1) n = sizeof(info->build) - 1;
    rd(R_STRDATA, (uint8_t *)info->build, n);
    info->build[n] = 0;
  }
  return 1;
}

uint8_t zf_reset(void)
{
  uint16_t t0;
  cmd(CMD_RESET);
  t0 = net_ticks();
  // the module restarts: anything may come back until STATUS = RESET
  while ((uint16_t)(net_ticks() - t0) < 50 * 10)
    if (rd8(R_STATUS) == S_RESET) return ZF_OK;
  return ZF_TIMEOUT;
}

uint8_t zf_netstate(void)
{
  if (run(CMD_NETSTATE, 50)) return 0xFF;
  return rd8(R_STR);
}

uint8_t zf_own_ip(char *ip)
{
  uint8_t b[4], i;
  char *p = ip;
  ip[0] = 0;
  if (run(CMD_GET_IP, 50)) return ZF_ERROR;
  rd(R_OWN_IP, b, 4);
  if (!(b[0] | b[1] | b[2] | b[3])) return ZF_OK;
  for (i = 0; i < 4; i++) {
    uint8_t v = b[i];
    if (v >= 100) *p++ = '0' + v / 100;
    if (v >= 10) *p++ = '0' + v / 10 % 10;
    *p++ = '0' + v % 10;
    *p++ = i < 3 ? '.' : 0;
  }
  return ZF_OK;
}

// reads DATA_SIZE bytes of a DATA_S2M answer into at_ring (at most AT_RING); returns the count
static uint16_t read_data(void)
{
  uint16_t n = (uint16_t)rd32(R_SIZE), i;
  uint8_t *p = at_ring_map();
  if (n > AT_RING) n = AT_RING;
  SPI_CTRL = CS_ON;
  SPI_DATA = C_RD_DATA;
  (void)SPI_DATA;
  (void)SPI_DATA;
  (void)SPI_DATA;
  for (i = 0; i < n; i++) *p++ = SPI_DATA;
  SPI_CTRL = CS_OFF;
  end_xfer(C_R_END);
  return n;
}

uint8_t zf_scan(kg_wifi_ap_t *aps, uint8_t *count, uint8_t max)
{
  uint8_t s, i, num, *p;
  uint16_t n;
  *count = 0;
  cmd(CMD_WSCAN);
  s = wait(50 * 15, 0xFF);
  if (s != S_S2M) return result(s);
  n = read_data();
  wait(50, S_S2M);
  // NUM_APS, then AUTH, RSSI, CHAN, SSID_LEN, SSID per network
  {
  uint8_t *ring = at_ring_map();
  p = ring;
  num = n ? *p++ : 0;
  for (i = 0; i < num && p + 4 <= ring + n; i++) {
    kg_wifi_ap_t ap;
    uint8_t len = p[3];
    if (p + 4 + len > ring + n) break;
    ap.ecn = p[0];
    ap.rssi = (int8_t)p[1];
    if (len > KG_WIFI_SSID_MAX) len = KG_WIFI_SSID_MAX;
    memcpy(ap.ssid, p + 4, len);
    ap.ssid[len] = 0;
    p += 4 + p[3];
    kg_wifi_ap_add(aps, count, max, &ap);
  }
  }
  return ZF_OK;
}

static uint8_t set_str(uint8_t c, const char *s)
{
  uint8_t b[62], n = (uint8_t)strlen(s);
  if (n > 61) n = 61;
  b[0] = n;
  memcpy(b + 1, s, n);
  wr(R_STR, b, n + 1);
  return run(c, 50);
}

uint8_t zf_join(const char *ssid, const char *pass)
{
  uint8_t r;
  if ((r = set_str(CMD_AP_NAME, ssid))) return r;
  if ((r = set_str(CMD_AP_PWD, pass))) return r;
  if ((r = run(CMD_AP_CONNECT, 50 * 30))) return r;
  return zf_netstate() == 2 ? ZF_OK : ZF_ERROR;
}

uint8_t zf_leave(void)
{
  return run(CMD_AP_DISC, 50 * 5);
}

// The firmware sends "/" + the selector of "gopher://host:port/T/rest" ("T/" cut), so a selector
// that starts with "/" goes through exactly; an empty one becomes "/"; others cannot be sent.
uint8_t zf_gopher_open(const char *host, uint16_t port, const char *sel)
{
  char *u = (char *)at_ring_map();
  uint8_t s, i;
  uint16_t n;
  if (sel[0] && sel[0] != '/') return ZF_NOSEL;
  if (strlen(host) + strlen(sel) + 20 > AT_RING) return ZF_ERROR;
  strcpy(u, "gopher://");
  strcat(u, host);
  if (port != 70) {
    char b[7], *q = b + 6;
    *q = 0;
    do { *--q = '0' + port % 10; port /= 10; } while (port);
    *--q = ':';
    strcat(u, q);
  }
  strcat(u, sel[0] ? "/1" : "/");
  strcat(u, sel);
  n = (uint16_t)strlen(u);
  {
    uint8_t b[4];
    b[0] = (uint8_t)n; b[1] = (uint8_t)(n >> 8); b[2] = b[3] = 0;
    wr(R_SIZE, b, 4);
  }
  cmd(CMD_SET_URL);
  s = wait(50 * 2, 0xFF);
  if (s != S_M2S) return result(s);
  SPI_CTRL = CS_ON;
  SPI_DATA = C_WR_DATA;
  (void)SPI_DATA;
  (void)SPI_DATA;
  for (i = 0; i < n; i++) SPI_DATA = (uint8_t)u[i];
  SPI_CTRL = CS_OFF;
  end_xfer(C_W_END);
  s = wait(50 * 2, S_M2S);
  if (s != S_READY) return result(s);
  chunk_len = chunk_pos = 0;
  at_end = stop_req = broken = 0;
  return run(CMD_G_STREAM, 50 * 20);
}

int16_t zf_gopher_read(uint8_t *buf, uint16_t max)
{
  uint8_t s;
  uint16_t n;
  if (chunk_pos == chunk_len) {
    if (at_end) return -1;
    cmd(CMD_S_READ);
    soft = 1;
    s = wait(50 * 32, 0xFF);                           // the firmware waits up to 30 s for data
    chunk_len = chunk_pos = 0;
    if (s == S_S2M) {
      chunk_len = read_data();
      wait(50, S_S2M);
    }
    soft = 0;
    if (stop_req) {                                    // Esc: the chunk is dropped
      if (s != S_S2M && s != S_READY) broken = 1;
      at_end = 1;
      return -3;
    }
    if (s == S_READY) { at_end = 1; return -1; }       // DATA_SIZE 0: the end (or 30 s of silence)
    if (s != S_S2M) { result(s); at_end = 1; return -2; }
    if (!chunk_len) { at_end = 1; return -1; }
  }
  n = chunk_len - chunk_pos;
  if (n > max) n = max;
  memcpy(buf, at_ring_map() + chunk_pos, n);
  chunk_pos += n;
  return (int16_t)n;
}

uint8_t zf_gopher_close(void)
{
  if (broken) {
    broken = 0;
    zf_reset();
    return 1;
  }
  run(CMD_S_CLOSE, 50 * 2);
  return 0;
}

// ---- tracker (XM / MOD / S3M objects; the codes of FEAT 10+)

static uint8_t trk_handle = 0xFF;

static void wr32(uint8_t reg, uint32_t v)
{
  uint8_t b[4];
  b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); b[2] = (uint8_t)(v >> 16); b[3] = (uint8_t)(v >> 24);
  wr(reg, b, 4);
}

void zf_track_free(void)
{
  uint8_t h = trk_handle;
  if (h == 0xFF) return;
  run(CMD_TRACK_STOP, 50 * 2);
  wr(R_HANDLE, &h, 1);
  run(CMD_DELETE_OBJ, 50 * 2);
  trk_handle = 0xFF;
}

// the module of the big buffer (len bytes) into an object of type (ZF_XM / ZF_MOD / ZF_S3M), then
// TRACK_INIT and TRACK_PLAY
uint8_t zf_track_play(uint8_t type, uint32_t len)
{
  uint32_t off = 0;
  uint8_t r, s, h;
  zf_track_free();
  len = (len + 3) & ~3UL;                                  // DMA of the ESP: sizes in 4-byte units
  wr32(R_SIZE, len);
  wr(R_OBJTYPE, &type, 1);
  if ((r = run(CMD_MAKE_OBJECT, 50 * 5))) return r;
  h = rd8(R_HANDLE);
  trk_handle = h;
  while (off < len) {
    uint16_t k = len - off > 0x4000 ? 0x4000 : (uint16_t)(len - off), i;
    const uint8_t *p;
    wr(R_HANDLE, &h, 1);
    wr32(R_OFFSET, off);
    wr32(R_SIZE, k);
    cmd(CMD_WRITE_OBJECT);
    s = wait(50 * 5, 0xFF);
    if (s != S_M2S) return result(s);
    PAGE3(PG_BIG + (uint8_t)(off >> 14));                 // a whole page: offsets are 16 KB aligned
    p = WIN3;
    SPI_CTRL = CS_ON;
    SPI_DATA = C_WR_DATA;
    (void)SPI_DATA;
    (void)SPI_DATA;
    for (i = 0; i < k; i++) SPI_DATA = *p++;
    SPI_CTRL = CS_OFF;
    end_xfer(C_W_END);
    s = wait(50 * 5, S_M2S);
    if (s != S_READY) return result(s);
    off += k;
  }
  wr(R_HANDLE, &h, 1);
  if ((r = run(CMD_TRACK_INIT, 50 * 30))) return r;       // an XM is parsed: seconds
  wr(R_HANDLE, &h, 1);
  return run(CMD_TRACK_PLAY, 50 * 2);
}

uint8_t zf_track_pause(uint8_t on)
{
  uint8_t h = trk_handle;
  if (h == 0xFF) return ZF_ERROR;
  if (on) return run(CMD_TRACK_STOP, 50 * 2);
  wr(R_HANDLE, &h, 1);
  return run(CMD_TRACK_PLAY, 50 * 2);
}
