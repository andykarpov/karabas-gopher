// Banked code (crt0.s): a bank is 32 KB at #4000-#BFFF (windows 1 + 2), bank n = pages #30 + 2n, #31 + 2n.
// Window 0 holds the common code (#0100-#1FFF) and all data + stack (#2000-#3FFF).
// BANKED functions may be called from any bank. A pointer passed to another bank must point to RAM
// or to the common code: constants of a bank (string literals too) are gone while another bank is in.
#pragma once
#ifdef __SDCC
#define BANKED __banked
#else
#define BANKED
#endif

#define BANK_FS   1      // FatFs, SD, INI, config
#define BANK_UI   2      // screens
