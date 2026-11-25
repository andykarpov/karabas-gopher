// pic.c on the host: pic_host FILE FORMAT OUT -> OUT = vconf (1) + palette (512) + 16 pages of the buffer
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pic_host_ts.h"
#include "../../src/pic.h"
uint8_t host_mem[256][0x4000], host_page3, host_cram[512], host_vpage, host_vconf, host_fm;
int main(int argc, char **argv)
{
  FILE *f;
  if (argc < 4) return 2;
  f = fopen(argv[1], "rb");
  long n = 0; int c; uint8_t r;
  while ((c = fgetc(f)) != EOF) { host_mem[0x80 + n / 0x4000][n % 0x4000] = (uint8_t)c; n++; }
  fclose(f);
  r = pic_decode((uint8_t)atoi(argv[2]), (uint32_t)n, 1);
  if (r) { printf("error %d\n", r); return 1; }
  pic_show(1);
  f = fopen(argv[3], "wb");
  fputc(host_vconf, f);
  fwrite(host_cram, 1, 512, f);
  fwrite(host_mem[0x70], 1, 16 * 0x4000, f);
  fclose(f);
  return 0;
}
