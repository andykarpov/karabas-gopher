// The AT driver on the host, over a tty (tools/at_proxy.py --pty, or tests/test_at_driver.py).
//   at_host TTY probe | status | scan | join SSID PASS | forget | get HOST PORT SELECTOR
#define _DEFAULT_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include "../../src/net/at.h"

static int fd;
static uint8_t ring_mem[AT_RING];
uint8_t *at_ring_map(void) { return ring_mem; }
static uint8_t pend[256];
static int npend;

static uint8_t t_avail(void)
{
  if (!npend) { int n = read(fd, pend, 191); if (n > 0) npend = n; }
  return (uint8_t)npend;
}
static void t_read(uint8_t *buf, uint8_t n) { memcpy(buf, pend, n); memmove(pend, pend + n, npend - n); npend -= n; }
static uint8_t t_room(void) { return 255; }
static void t_write(const uint8_t *buf, uint8_t n) { while (n) { int w = write(fd, buf, n); if (w > 0) { buf += w; n -= w; } } }
static void t_clear(void) { npend = 0; }
static const transport_t tr = { t_avail, t_read, t_room, t_write, t_clear };

uint16_t net_ticks(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint16_t)(ts.tv_sec * 50 + ts.tv_nsec / 20000000);
}
uint8_t net_idle(void) { usleep(1000); return 0; }

int main(int argc, char **argv)
{
  struct termios t;
  if (argc < 3) return 2;
  fd = open(argv[1], O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd < 0) { perror(argv[1]); return 2; }
  tcgetattr(fd, &t); cfmakeraw(&t); tcsetattr(fd, TCSANOW, &t);
  at_begin(&tr);
  if (!at_probe()) { printf("no modem\n"); return 1; }
  if (!strcmp(argv[2], "probe")) { printf("modem ok\n"); return 0; }
  if (!strcmp(argv[2], "status")) {
    at_status_t st;
    uint8_t r = at_status(&st);
    printf("r=%d version=%s mode=%d flow=%d ssid=%s ip=%s\n", r, st.version, st.mode, st.flow, st.ssid, st.ip);
    return r;
  }
  if (!strcmp(argv[2], "scan")) {
    kg_wifi_ap_t aps[AT_APS]; uint8_t n, i, r = at_scan(aps, &n);
    for (i = 0; i < n; i++) printf("ap %s %d %d\n", aps[i].ssid, aps[i].rssi, aps[i].ecn);
    return r;
  }
  if (!strcmp(argv[2], "join") && argc > 4) {
    int8_t e; uint8_t r = at_join(argv[3], argv[4], &e);
    printf("r=%d err=%d\n", r, e);
    return r;
  }
  if (!strcmp(argv[2], "forget")) return at_forget();
  if (!strcmp(argv[2], "get") && argc > 5) {
    char req[300]; uint8_t buf[100]; int16_t n; long total = 0;
    uint8_t r = at_tcp_open(argv[3], (uint16_t)atoi(argv[4]));
    if (r) { printf("open r=%d\n", r); return r; }
    snprintf(req, sizeof(req), "%s\r\n", argv[5]);
    if ((r = at_tcp_send((const uint8_t *)req, (uint16_t)strlen(req)))) { printf("send r=%d\n", r); return r; }
    uint16_t t0 = net_ticks();
    while ((n = at_tcp_recv(buf, sizeof(buf))) >= 0) {
      if (n) { fwrite(buf, 1, n, stdout); total += n; t0 = net_ticks(); }
      else if ((uint16_t)(net_ticks() - t0) > 50 * 10) { fprintf(stderr, "recv timeout\n"); return 3; }
      else net_idle();
    }
    fflush(stdout);
    fprintf(stderr, "total %ld\n", total);
    return 0;
  }
  return 2;
}
