#include <stdlib.h>
#include "check.h"
#include "../../src/gopher/url.h"
#include "../../src/gopher/menu.h"
#include "../../src/gopher/cp.h"
#include "../../src/gopher/doc.h"

static uint8_t pages[DOC_PAGES][0x4000], idx[DOC_IDX_PG][0x4000];
uint8_t *doc_map(uint8_t p) { return p >= DOC_IDX ? idx[p - DOC_IDX] : pages[p]; }

static const char *row(uint16_t i) { static char b[300]; b[doc_row(i, b, 299)] = 0; return b; }

int main(void)
{
  gurl_t u;
  char s[300];

  // URLs
  CHECK(url_parse("gopher://alexspawn.ru", &u));
  CHECK_STR(u.host, "alexspawn.ru"); CHECK_EQ(u.port, 70); CHECK_EQ(u.type, '1'); CHECK_STR(u.sel, "");
  CHECK(url_parse("gopher://h.net:7070/0/a/b.txt", &u));
  CHECK_STR(u.host, "h.net"); CHECK_EQ(u.port, 7070); CHECK_EQ(u.type, '0'); CHECK_STR(u.sel, "/a/b.txt");
  CHECK(url_parse("host/7/search%09zx%20spectrum", &u));
  CHECK_EQ(u.type, '7'); CHECK_STR(u.sel, "/search\tzx spectrum");
  CHECK(url_parse("gopher://h/1", &u)); CHECK_STR(u.sel, "");
  CHECK(url_parse("gopher://h/1foo", &u)); CHECK_STR(u.sel, "foo");   // no leading '/'
  CHECK(!url_parse("gopher://", &u));
  url_parse("gopher://h.net:7070/0/a b%25", &u);
  url_format(&u, s, sizeof(s)); CHECK_STR(s, "gopher://h.net:7070/0/a%20b%25");
  url_parse("h", &u); url_format(&u, s, sizeof(s)); CHECK_STR(s, "gopher://h/");
  url_parse("gopher://h/1/x", &u); url_format(&u, s, 14); CHECK_STR(s, "gopher://h/1/");

  // download names
  url_filename("/pub/zx/game.tap", s, 64); CHECK_STR(s, "game.tap");
  url_filename("/pub/zx/", s, 64); CHECK_STR(s, "zx");
  url_filename("", s, 64); CHECK_STR(s, "file");
  url_filename("/", s, 64); CHECK_STR(s, "file");
  url_filename("/a/b?c:d*e.scr\tquery", s, 64); CHECK_STR(s, "b_c_d_e.scr");
  url_filename("x\\win\\name.txt", s, 64); CHECK_STR(s, "name.txt");
  url_filename("/very-long-file-name.scr", s, 12); CHECK_STR(s, "very-lo.scr");
  url_filename("/abcdefghijklmnop", s, 8); CHECK_STR(s, "abcdefg");
  url_filename("/ .hidden. ", s, 64); CHECK_STR(s, "hidden");

  // menu lines
  gitem_t it;
  const char *l = "1MODs Archive\t/mod\talexspawn.ru\t70";
  CHECK(menu_item(l, strlen(l), &it));
  CHECK_EQ(it.type, '1'); CHECK_EQ(it.dlen, 12); CHECK(!strncmp(it.disp, "MODs Archive", 12));
  CHECK_EQ(it.slen, 4); CHECK(!strncmp(it.sel, "/mod", 4));
  CHECK_EQ(it.hlen, 12); CHECK_EQ(it.port, 70);
  l = "iinfo only";
  CHECK(menu_item(l, strlen(l), &it)); CHECK_EQ(it.dlen, 9); CHECK_EQ(it.slen, 0); CHECK_EQ(it.port, 70);
  CHECK(!menu_item(".", 1, &it));
  CHECK(!menu_item("", 0, &it));
  l = "1x\t\th\t7000\t+";
  CHECK(menu_item(l, strlen(l), &it)); CHECK_EQ(it.port, 7000); CHECK_EQ(it.slen, 0);
  CHECK(menu_link('1')); CHECK(menu_link('9')); CHECK(!menu_link('i')); CHECK(!menu_link('3'));

  // charsets
  CHECK_EQ(cp_conv(0, "\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82", 12, s, 80), 6);
  CHECK_STR(s, "\x8f\xe0\xa8\xa2\xa5\xe2");                     // Привет
  cp_conv(0, "\xd1\x91\xd0\x81 \xe2\x80\x94 \xc2\xab", 11, s, 80);
  CHECK_STR(s, "\xf1\xf0 - \"");                                  // ёЁ — «
  cp_conv(0, "a\xd0", 2, s, 80); CHECK_STR(s, "a");               // cut sequence dropped
  cp_conv(0, "\xf0\x9f\x98\x80x", 5, s, 80); CHECK_STR(s, "?x");   // emoji
  cp_conv(1, "\xcf\xf0\xe8\xe2\xe5\xf2", 6, s, 80); CHECK_STR(s, "\x8f\xe0\xa8\xa2\xa5\xe2");   // cp1251
  cp_conv(2, "\xf0\xd2\xc9\xd7\xc5\xd4", 6, s, 80); CHECK_STR(s, "\x8f\xe0\xa8\xa2\xa5\xe2");   // koi8-r
  cp_conv(3, "\x8f\xe0", 2, s, 80); CHECK_STR(s, "\x8f\xe0");
  cp_conv(0, "a\tb", 3, s, 80); CHECK_STR(s, "a       b");
  CHECK_EQ(cp_conv(0, "abcdef", 6, s, 4), 4); CHECK_STR(s, "abcd");

  // doc: menu
  const char *m = "iHello\t\tnull\t1\r\n1Docs\t/d\th\t70\r\n.\r\n";
  doc_reset(0, 0); CHECK(doc_append((const uint8_t *)m, strlen(m))); doc_end();
  CHECK_EQ(doc_rows(), 2); CHECK_STR(row(0), "iHello\t\tnull\t1"); CHECK_STR(row(1), "1Docs\t/d\th\t70");
  // doc: text wrapped at 10 columns, UTF-8 counts characters
  const char *t = "0123456789ABC\r\n\xd0\x9f\xd0\x9f\xd0\x9f\xd0\x9f\xd0\x9f\xd0\x9f\xd0\x9f\xd0\x9f\xd0\x9f\xd0\x9fZ\n\nx\tyz\n.\n";
  doc_reset(0, 10); doc_append((const uint8_t *)t, strlen(t)); doc_end();
  CHECK_EQ(doc_rows(), 6);
  CHECK_STR(row(0), "0123456789"); CHECK_STR(row(1), "ABC"); CHECK_EQ(strlen(row(2)), 20);
  CHECK_STR(row(3), "Z"); CHECK_STR(row(4), ""); CHECK_STR(row(5), "x\tyz");
  // doc: pieces cut anywhere give the same rows
  doc_reset(0, 10);
  for (size_t i = 0; i < strlen(t); i++) doc_append((const uint8_t *)t + i, 1);
  doc_end(); CHECK_EQ(doc_rows(), 6); CHECK_STR(row(3), "Z");
  // doc: across pages, and full
  static uint8_t big[0x4000];
  doc_reset(3, 0);
  for (int i = 0; i < 0x4000; i++) big[i] = i % 100 == 99 ? '\n' : 'a' + i % 26;
  CHECK(doc_append(big, 0x3FF0)); CHECK(doc_append(big, 100));
  // 163 lines of 99, then one of 68 + 99 that crosses into page 1
  CHECK_EQ(doc_rows(), 164); CHECK_EQ(strlen(row(162)), 99); CHECK_EQ(strlen(row(163)), 167);
  for (int i = 0; i < DOC_PAGES; i++) doc_append(big, 0x4000);
  CHECK(doc_full()); CHECK(!doc_append(big, 1));
  DONE();
}
