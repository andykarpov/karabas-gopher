// dl.c (downloads) on the real FatFs over a FAT image in memory (build/dl.img, made by mkfs.fat).
// Built from build/ after iconv to CP866 (the Makefile), hence the include paths.
#include <stdlib.h>
#include "../tests/host/check.h"
#include "../src/dl.h"
#include "../src/fatfs/ff.h"
#include "../src/fatfs/diskio.h"

static unsigned char *img;
static long img_size;

DSTATUS disk_status(BYTE d) { (void)d; return 0; }
DSTATUS disk_initialize(BYTE d) { (void)d; return 0; }
DRESULT disk_read(BYTE d, BYTE *b, LBA_t s, UINT n) { (void)d; memcpy(b, img + s * 512, n * 512); return RES_OK; }
DRESULT disk_write(BYTE d, const BYTE *b, LBA_t s, UINT n) { (void)d; memcpy(img + s * 512, b, n * 512); return RES_OK; }
DRESULT disk_ioctl(BYTE d, BYTE c, void *b) { (void)d; (void)b; return c == CTRL_SYNC ? RES_OK : RES_PARERR; }

static long fsize(const char *p) { FILINFO fi; return f_stat(p, &fi) ? -1 : (long)fi.fsize; }

static uint8_t save(char *path, const char *data, uint8_t cut)
{
  uint8_t r = dl_open(path, 100);
  if (r) return r;
  if ((r = dl_write((const uint8_t *)data, (uint16_t)strlen(data)))) return r;
  return dl_close(1, cut);
}

int main(int argc, char **argv)
{
  FATFS fs;
  char p[100];
  FILE *f = fopen(argc > 1 ? argv[1] : "build/dl.img", "rb");
  if (!f) { perror("image"); return 2; }
  fseek(f, 0, SEEK_END); img_size = ftell(f); rewind(f);
  img = malloc(img_size);
  if (fread(img, 1, img_size, f) != (size_t)img_size) return 2;
  fclose(f);
  CHECK_EQ(f_mount(&fs, "", 1), FR_OK);

  strcpy(p, "/downloads/game.tap");                 // the dir is made
  CHECK_EQ(save(p, "12345", 0), 0); CHECK_STR(p, "/downloads/game.tap"); CHECK_EQ(fsize(p), 5);
  strcpy(p, "/downloads/game.tap");                 // taken: _1, _2
  CHECK_EQ(save(p, "ab", 0), 0); CHECK_STR(p, "/downloads/game_1.tap"); CHECK_EQ(fsize(p), 2);
  strcpy(p, "/downloads/game.tap");
  CHECK_EQ(save(p, "ab", 0), 0); CHECK_STR(p, "/downloads/game_2.tap");
  strcpy(p, "/downloads/readme");                   // no extension
  CHECK_EQ(save(p, "x", 0), 0); strcpy(p, "/downloads/readme");
  CHECK_EQ(save(p, "y", 0), 0); CHECK_STR(p, "/downloads/readme_1");
  strcpy(p, "/downloads/a.txt");                    // the gopher "." line cut
  CHECK_EQ(save(p, "line\r\n.\r\n", 3), 0); CHECK_EQ(fsize(p), 6);
  strcpy(p, "/downloads/cancel.bin");               // cancelled: deleted
  CHECK_EQ(dl_open(p, 100), 0);
  CHECK_EQ(dl_write((const uint8_t *)"zz", 2), 0);
  CHECK_EQ(dl_close(0, 0), 0); CHECK_EQ(fsize(p), -1);
  CHECK_EQ(dl_close(0, 0), 0);                      // nothing open: no-op
  strcpy(p, "/downloads/2026_11_25/x.scr");        // two levels made
  CHECK_EQ(save(p, "abc", 0), 0); CHECK_EQ(fsize(p), 3);
  strcpy(p, "/downloads/Тест.scr");                 // CP866 name (the source is converted)
  CHECK_EQ(save(p, "s", 0), 0); CHECK_EQ(fsize(p), 1);
  // like a real download: 300 KB in 128-byte pieces (the receive buffer), read back
  {
    static uint8_t chunk[128];
    uint32_t i, sum = 0, back = 0;
    FIL rf; UINT n;
    strcpy(p, "/downloads/big.mod");
    CHECK_EQ(dl_open(p, 100), 0);
    for (i = 0; i < 300 * 1024 / 128; i++) {
      uint8_t j;
      for (j = 0; j < 128; j++) { chunk[j] = (uint8_t)(i * 7 + j); sum += chunk[j]; }
      if (dl_write(chunk, 128)) break;
    }
    CHECK_EQ(dl_close(1, 0), 0);
    CHECK_EQ(fsize(p), 300 * 1024);
    CHECK_EQ(f_open(&rf, p, FA_READ), 0);
    while (!f_read(&rf, chunk, sizeof(chunk), &n) && n) for (i = 0; i < n; i++) back += chunk[i];
    f_close(&rf);
    CHECK_EQ(back, sum);
  }
  // many long names in one folder (it grows over clusters), every third deleted, the holes reused
  {
    int i, bad = 0;
    for (i = 0; i < 120; i++) {
      sprintf(p, "/downloads/many/a_rather_long_file_name_number_%03d.pt3", i);
      if (i % 3 == 1) { if (dl_open(p, 100) || dl_write((const uint8_t *)"x", 1) || dl_close(0, 0)) bad++; }
      else if (save(p, "music", 0)) bad++;
    }
    for (i = 0; i < 40; i++) {
      sprintf(p, "/downloads/many/another_name_%02d.scr", i);
      if (save(p, "screen", 0)) bad++;
    }
    CHECK_EQ(bad, 0);
    sprintf(p, "/downloads/many/a_rather_long_file_name_number_%03d.pt3", 1);
    CHECK_EQ(fsize(p), -1);
    sprintf(p, "/downloads/many/a_rather_long_file_name_number_%03d.pt3", 119);
    CHECK_EQ(fsize(p), 5);
  }
  // the image back to its file: the Makefile runs fsck.fat -n on it
  f = fopen(argc > 1 ? argv[1] : "build/dl.img", "wb");
  if (!f || fwrite(img, 1, img_size, f) != (size_t)img_size) return 2;
  fclose(f);
  DONE();
}
