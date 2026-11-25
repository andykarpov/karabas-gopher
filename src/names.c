// Tables both banks use: they must live in the common code (bank.h).
#include "config.h"

const char *const backend_names[BK_COUNT] = { "auto", "zifi32", "zifi", "usb" };
const char *const charset_names[CS_COUNT] = { "utf8", "cp1251", "koi8", "cp866" };
const char *const yes_no[2] = { "нет", "да" };
