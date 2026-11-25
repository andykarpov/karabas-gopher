#include "check.h"
#include "../../src/edit.h"
#include "../../src/kbd.h"

int main(void)
{
  char b[9] = "abc";
  edit_t e;
  edit_start(&e, b, 8, 4);
  CHECK_EQ(e.pos, 3); CHECK_EQ(e.len, 3);
  edit_key(&e, 'd'); edit_key(&e, 'e');
  CHECK_STR(b, "abcde"); CHECK_EQ(e.scroll, 2);
  edit_key(&e, K_HOME); CHECK_EQ(e.scroll, 0);
  edit_key(&e, 'X'); CHECK_STR(b, "Xabcde");
  edit_key(&e, K_DEL); CHECK_STR(b, "Xbcde");
  edit_key(&e, K_BS); CHECK_STR(b, "bcde"); CHECK_EQ(e.pos, 0);
  edit_key(&e, K_BS); CHECK_STR(b, "bcde");
  edit_key(&e, K_END); edit_key(&e, K_DEL); CHECK_STR(b, "bcde");
  for (int i = 0; i < 10; i++) edit_key(&e, '1');
  CHECK_STR(b, "bcde1111"); CHECK_EQ(e.len, 8);
  edit_key(&e, K_LEFT); edit_key(&e, K_BS); CHECK_STR(b, "bcde111");
  edit_key(&e, 0xA9); CHECK_STR(b, "bcde11\xA9" "1");
  edit_key(&e, K_UP); edit_key(&e, K_F1); CHECK_STR(b, "bcde11\xA9" "1");
  edit_key(&e, 'y' | KM_CTRL); CHECK_STR(b, ""); CHECK_EQ(e.pos, 0);
  CHECK_EQ(edit_key(&e, K_ENTER), EDIT_DONE);
  CHECK_EQ(edit_key(&e, K_ESC), EDIT_CANCEL);
  DONE();
}
