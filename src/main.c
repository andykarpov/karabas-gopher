// karabas gopher: start-up. Ф1: text mode, keyboard, SD + FatFs, settings in /zifi/gopher.ini.
#include "ts.h"
#include "text.h"
#include "kbd.h"
#include "config.h"
#include "settings.h"
#include "fatfs/ff.h"

// the messages live in the common code: settings_msg() runs in another bank (bank.h)
static const char m_nosd[] = "Нет SD-карты: настройки не сохранятся";
static const char m_nofile[] = "Файла " CFG_PATH " нет: настройки по умолчанию, F2 создаст его";
static const char m_err[] = "Ошибка чтения " CFG_PATH;
static const char m_ok[] = "Настройки прочитаны из " CFG_PATH;

void main(void)
{
  uint8_t r;
  txt_init();
  kbd_init();
  __asm__("ei");

  cfg_defaults();
  r = cfg_mount();
  if (r)
    settings_msg(m_nosd, r);
  else if ((r = cfg_load()) == FR_NO_FILE || r == FR_NO_PATH) {
    settings_msg(m_nofile, 0xFF);
    if (cfg_zifi_read()) settings_offer_zifi();
  }
  else if (r)
    settings_msg(m_err, r);
  else
    settings_msg(m_ok, 0xFF);
  settings_run();
}
