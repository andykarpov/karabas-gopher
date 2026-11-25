#include "check.h"
#include "../../src/ini.h"

static char buf[512];
static ini_t ini;
static void load(const char *t) { strcpy(buf, t); ini_init(&ini, buf, sizeof(buf), strlen(t)); }

int main(void)
{
  char v[64];
  load("; comment\n[Net]\n backend = zifi \n[zifi32]\nssid=home;net\npassword = p=w ;x\n\n# end\n");
  CHECK(ini_get(&ini, "net", "BACKEND", v, sizeof(v))); CHECK_STR(v, "zifi");
  CHECK(ini_get(&ini, "zifi32", "ssid", v, sizeof(v))); CHECK_STR(v, "home;net");
  CHECK(ini_get(&ini, "zifi32", "password", v, sizeof(v))); CHECK_STR(v, "p=w ;x");
  CHECK(!ini_get(&ini, "net", "ssid", v, sizeof(v)));
  CHECK(!ini_get(&ini, "none", "ssid", v, sizeof(v)));
  CHECK(ini_get(&ini, "zifi32", "ssid", v, 5)); CHECK_STR(v, "home");

  CHECK(ini_set(&ini, "net", "backend", "usb"));
  CHECK_STR(buf, "; comment\n[Net]\n backend = usb \n[zifi32]\nssid=home;net\npassword = p=w ;x\n\n# end\n");
  CHECK(ini_set(&ini, "net", "speed", "1"));
  CHECK_STR(buf, "; comment\n[Net]\n backend = usb \nspeed=1\n[zifi32]\nssid=home;net\npassword = p=w ;x\n\n# end\n");
  CHECK(ini_set(&ini, "zifi32", "autoconnect", "1"));
  CHECK_STR(buf, "; comment\n[Net]\n backend = usb \nspeed=1\n[zifi32]\nssid=home;net\npassword = p=w ;x\nautoconnect=1\n\n# end\n");
  CHECK(ini_set(&ini, "gopher", "home", "gopher://x"));
  CHECK_STR(buf, "; comment\n[Net]\n backend = usb \nspeed=1\n[zifi32]\nssid=home;net\npassword = p=w ;x\nautoconnect=1\n\n# end\n\n[gopher]\nhome=gopher://x\n");
  CHECK(ini_set(&ini, "zifi32", "password", ""));
  CHECK(ini_get(&ini, "zifi32", "password", v, sizeof(v))); CHECK_STR(v, "");

  load("[a]\r\nk=1");                         // CR LF and no line end at the end
  CHECK(ini_set(&ini, "a", "j", "2"));
  CHECK_STR(buf, "[a]\r\nk=1\r\nj=2\r\n");
  CHECK(ini_set(&ini, "b", "x", "3"));
  CHECK_STR(buf, "[a]\r\nk=1\r\nj=2\r\n\r\n[b]\r\nx=3\r\n");

  load("");
  CHECK(ini_set(&ini, "s", "k", "v"));
  CHECK_STR(buf, "[s]\nk=v\n");

  char small[16]; strcpy(small, "[s]\nk=v\n");    // full buffer: unchanged
  ini_init(&ini, small, sizeof(small), strlen(small));
  CHECK(!ini_set(&ini, "s", "k", "0123456789"));
  CHECK_STR(small, "[s]\nk=v\n");
  CHECK(ini_set(&ini, "s", "k", "0123456"));
  CHECK_STR(small, "[s]\nk=0123456\n");
  DONE();
}
