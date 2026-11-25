#include <string.h>
#include "dl.h"
#include "fatfs/ff.h"

static FIL df;
static char *dpath;

uint8_t dl_open(char *path, uint8_t room) BANKED
{
  char *slash = strrchr(path, '/'), *dot, ext[16];
  uint8_t r, i, n;
  dpath = 0;
  r = f_open(&df, path, FA_WRITE | FA_CREATE_NEW);
  if (r == FR_NO_PATH && slash && slash != path) {
    *slash = 0;
    r = f_mkdir(path);
    *slash = '/';
    if (r && r != FR_EXIST) return r;
    r = f_open(&df, path, FA_WRITE | FA_CREATE_NEW);
  }
  if (r == FR_EXIST) {
    if (!slash) slash = path - 1;
    dot = strrchr(slash + 1, '.');
    if (!dot || dot == slash + 1 || strlen(dot) >= sizeof(ext)) dot = path + strlen(path);
    strcpy(ext, dot);
    n = (uint8_t)(dot - path);
    if (n + 4 + strlen(ext) > room) return r;
    for (i = 1; i < 100 && r == FR_EXIST; i++) {
      char *p = path + n;
      *p++ = '_';
      if (i >= 10) *p++ = '0' + i / 10;
      *p++ = '0' + i % 10;
      strcpy(p, ext);
      r = f_open(&df, path, FA_WRITE | FA_CREATE_NEW);
    }
  }
  if (!r) dpath = path;
  return r;
}

uint8_t dl_write(const uint8_t *buf, uint16_t n) BANKED
{
  UINT w;
  uint8_t r = f_write(&df, buf, n, &w);
  return r ? r : w != n ? DL_FULL : 0;
}

uint8_t dl_close(uint8_t keep, uint8_t cut) BANKED
{
  uint8_t r = 0;
  if (!dpath) return 0;
  if (keep && cut && f_size(&df) >= cut) {
    r = f_lseek(&df, f_size(&df) - cut);
    if (!r) r = f_truncate(&df);
  }
  if (f_close(&df) && !r) r = FR_DISK_ERR;
  if (!keep) f_unlink(dpath);
  dpath = 0;
  return r;
}
