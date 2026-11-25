// SD card on the Z-controller of TS-Conf (the card Wild Commander uses) + the FatFs disk interface.
// #77 write: bit 0 = 1 always (0 lets the ESP32 take the FT / ESP bus on the Mini G), bit 1 = SD /CS;
// #57: OUT sends a byte, IN sends #FF and returns the byte received in that exchange.
#include <stdint.h>
#include "fatfs/ff.h"
#include "fatfs/diskio.h"

__sfr __at(0x57) SD_DAT;
__sfr __at(0x77) SD_CFG;

#define CS_ON()   (SD_CFG = 0x01)
#define CS_OFF()  (SD_CFG = 0x03)

static uint8_t sd_type;          // 0 = none, 1 = SDSC / MMC (byte address), 2 = SDHC (block address)
static DSTATUS sd_stat = STA_NOINIT;

// 512 bytes in / out (HL = buffer, sdcccall 1)
static void rx512(uint8_t *buf) __naked
{
  (void)buf;
  __asm
    ld  bc, #0x0057
    inir
    inir
    ret
  __endasm;
}

static void tx512(const uint8_t *buf) __naked
{
  (void)buf;
  __asm
    ld  bc, #0x0057
    otir
    otir
    ret
  __endasm;
}

static uint8_t wait_ready(void)
{
  uint32_t i = 300000;           // about 1 s at 14 MHz (a write may keep the card busy 250 ms+)
  while (SD_DAT != 0xFF)
    if (!--i) return 0;
  return 1;
}

static void deselect(void)
{
  CS_OFF();
  (void)SD_DAT;                  // one more byte of clocks: the card lets go of DO
}

static uint8_t select(void)
{
  CS_ON();
  (void)SD_DAT;
  if (wait_ready()) return 1;
  deselect();
  return 0;
}

static uint8_t cmd(uint8_t c, uint32_t arg)
{
  uint8_t r, n;
  if (c & 0x80) {                // ACMD = CMD55 + CMD
    c &= 0x7F;
    r = cmd(55, 0);
    if (r > 1) return r;
  }
  if (c != 12) {
    deselect();
    if (!select()) return 0xFF;
  }
  SD_DAT = 0x40 | c;
  SD_DAT = (uint8_t)(arg >> 24);
  SD_DAT = (uint8_t)(arg >> 16);
  SD_DAT = (uint8_t)(arg >> 8);
  SD_DAT = (uint8_t)arg;
  SD_DAT = c == 0 ? 0x95 : c == 8 ? 0x87 : 0x01;
  if (c == 12) (void)SD_DAT;     // skip a stuff byte
  n = 10;
  do r = SD_DAT; while ((r & 0x80) && --n);
  return r;
}

DSTATUS disk_status(BYTE drv)
{
  return drv ? STA_NOINIT : sd_stat;
}

DSTATUS disk_initialize(BYTE drv)
{
  uint8_t n, ocr[4], ty = 0;
  uint16_t t;
  if (drv) return STA_NOINIT;
  CS_OFF();
  for (n = 10; n; n--) (void)SD_DAT;       // 80 clocks with CS high
  if (cmd(0, 0) == 1) {
    if (cmd(8, 0x1AA) == 1) {              // SD v2
      for (n = 0; n < 4; n++) ocr[n] = SD_DAT;
      if (ocr[2] == 0x01 && ocr[3] == 0xAA) {
        for (t = 20000; t && cmd(0x80 | 41, 1UL << 30); t--) ;
        if (t && cmd(58, 0) == 0) {
          for (n = 0; n < 4; n++) ocr[n] = SD_DAT;
          ty = (ocr[0] & 0x40) ? 2 : 1;
        }
      }
    } else {                               // SD v1 or MMC
      uint8_t c = cmd(0x80 | 41, 0) <= 1 ? (0x80 | 41) : 1;
      for (t = 20000; t && cmd(c, 0); t--) ;
      if (t && cmd(16, 512) == 0) ty = 1;
    }
  }
  sd_type = ty;
  deselect();
  sd_stat = ty ? 0 : STA_NOINIT;
  return sd_stat;
}

DRESULT disk_read(BYTE drv, BYTE *buf, LBA_t sector, UINT count)
{
  uint16_t t;
  uint8_t r;
  if (drv || !count) return RES_PARERR;
  if (sd_stat & STA_NOINIT) return RES_NOTRDY;
  if (sd_type != 2) sector *= 512;
  while (count) {
    if (cmd(17, sector)) break;
    t = 50000;
    do r = SD_DAT; while (r == 0xFF && --t);
    if (r != 0xFE) break;                  // no data token
    rx512(buf);
    (void)SD_DAT; (void)SD_DAT;            // CRC
    buf += 512;
    sector += sd_type == 2 ? 1 : 512;
    count--;
  }
  deselect();
  return count ? RES_ERROR : RES_OK;
}

DRESULT disk_write(BYTE drv, const BYTE *buf, LBA_t sector, UINT count)
{
  uint8_t r, n;
  if (drv || !count) return RES_PARERR;
  if (sd_stat & STA_NOINIT) return RES_NOTRDY;
  if (sd_type != 2) sector *= 512;
  while (count) {
    if (cmd(24, sector)) break;
    SD_DAT = 0xFF;
    SD_DAT = 0xFE;                         // data token
    tx512(buf);
    SD_DAT = 0xFF; SD_DAT = 0xFF;          // CRC
    n = 8;                                 // the data response, xxx0 0101 = accepted
    do r = SD_DAT; while (r == 0xFF && --n);
    if ((r & 0x1F) != 0x05) break;
    if (!wait_ready()) break;
    buf += 512;
    sector += sd_type == 2 ? 1 : 512;
    count--;
  }
  deselect();
  return count ? RES_ERROR : RES_OK;
}

DRESULT disk_ioctl(BYTE drv, BYTE c, void *buf)
{
  (void)buf;
  if (drv) return RES_PARERR;
  if (c == CTRL_SYNC) {
    uint8_t ok = select();
    deselect();
    return ok ? RES_OK : RES_ERROR;
  }
  return RES_PARERR;
}
