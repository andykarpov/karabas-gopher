#include <string.h>
#include "bm.h"
#include "config.h"
#include "fatfs/ff.h"
#include "gopher/gopher.h"

static FIL bf;
static uint8_t bbuf[384];      // RAM: handed to bank 3; a URL of 255 + its escapes + a name

uint8_t bm_load(void) BANKED
{
  UINT n;
  uint8_t r;
  gopher_doc_reset(3, 0);
  r = f_open(&bf, BM_PATH, FA_READ);
  if (r) { gopher_doc_end(); return r; }
  while (!(r = f_read(&bf, bbuf, sizeof(bbuf), &n)) && n)
    gopher_doc_append(bbuf, (uint16_t)n);
  f_close(&bf);
  gopher_doc_end();
  return r;
}

uint8_t bm_add(const char *line) BANKED
{
  UINT n;
  uint8_t r = f_mkdir(CFG_DIR);
  if (r && r != FR_EXIST) return r;
  if ((r = f_open(&bf, BM_PATH, FA_WRITE | FA_OPEN_APPEND))) return r;
  r = f_write(&bf, line, (UINT)strlen(line), &n);
  if (!r) r = f_write(&bf, "\r\n", 2, &n);
  if (f_close(&bf) && !r) r = FR_DISK_ERR;
  return r;
}

uint8_t bm_delete(uint16_t row) BANKED
{
  UINT n;
  uint16_t i, rows = gopher_rows(), len;
  uint8_t r;
  if ((r = f_open(&bf, BM_PATH, FA_WRITE | FA_CREATE_ALWAYS))) return r;
  for (i = 0; i < rows && !r; i++) {
    if (i == row) continue;
    len = gopher_row(i, (char *)bbuf, sizeof(bbuf) - 2);
    bbuf[len] = '\r'; bbuf[len + 1] = '\n';
    r = f_write(&bf, bbuf, len + 2, &n);
  }
  if (f_close(&bf) && !r) r = FR_DISK_ERR;
  return r;
}
