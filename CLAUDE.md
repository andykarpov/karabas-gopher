# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

Our own Gopher browser for TS-Conf (`.spg`, Wild Commander / ts-bios), with the features of AlexSpawn's `gopher_esp32.spg` v0.3 (no sources, third party), and one network layer with autodetect and a choice of backend:

- **ZiFi32**: ESP32-S3 with the tslabs firmware (`pentevo/esp32`, SPI `ESP32_SPI`), Mini G only;
- **ZiFi**: ESP8266 with the stock AT firmware on the TS ZiFi UART (#C0EF / #C1EF / #C7EF), 115200, all boards;
- **USB serial**: TS RS232 / EVO RS232 (#C2EF.. / #F8EF..) through the MCU to the clean USB CDC (if02) or a USB-UART adapter.

Docs, in Russian: `docs/PLAN.md` (proposal, decisions, phases), `docs/BACKLOG.md` (open items, keep it current). Follow the phases; record a changed decision in PLAN.md. Status 2026-11-25: plan approved (decisions in PLAN §7: C on SDCC, PC AT proxy + real ESP8266 on a USB-UART adapter, own INI format, no AY backend, repo local only, name open: maybe `karabas-gopher`).

## Build and layout

- `make` -> `build/gopher.spg` (SDCC 4.2 `sdcc` / `sdasz80`, Python 3 tools; default sdcccall, no prebuilt tslabs SDK libs). Sources are UTF-8, the Makefile converts each to CP866 (`iconv`, build/*.866.c) before compiling. `make test` = host tests (gcc: `tests/host/test_*.c` for kbd / edit / ini; Python: `tests/test_*.py`), seconds. `make emu` = the spg in unreal-ng TS-Conf, headless via emu-shot -> `build/emu.png`. `tools/emu_dbg.py` = the same run plus WebAPI calls (`post:/pause`, `get:/registers`, `get:/memory/ram/PAGE/OFFSET?length=N`, `paths:WORD`) for debugging. `res/font866.bin` = the CP866 8x8 font of Wild Commander (zx-evo `pentevo/soft/WC/source/FONT32L3.CDB`, `make font`; user's choice 2026-11-25; arrows at #18-#1B, ■ at #FE); `make font-psf` = the older public-domain console font FullCyrSlav-VGA8.
- **Memory map and banks (`src/bank.h`, `src/crt0.s`):** window 0 = page #20: common code #0100-#1FFF + data and stack #2000-#3FFF (SP #4000); windows 1 + 2 = the current code bank of 32 KB (bank n = pages #30 + 2n, #31 + 2n; linked at n * #10000 + #4000 with `-Wl-b_BANKn`, files per bank in the Makefile `BANKn` lists, `--codeseg/--constseg BANKn`); window 3 = paged buffers and the text screen. Calls between banks: functions marked `BANKED` (SDCC `__banked`, our `___sdcc_bcall_ehl` / `set_bank` in crt0.s). A pointer handed to another bank must point to RAM or the common code (bank constants and string literals vanish with their bank). `tools/ihxsplit.py` checks the limits (common < #2000, data < #3C00, bank <= 32 KB) and writes the page images; the font is its own SPG block (page #25). Bank 1 = FatFs + SD + config (~31 KB, nearly full), bank 2 = screens (settings, ui, netui), bank 3 = network (`src/net`: tr_ts transports, AT driver `at.c`, parsers `esp_at_parse.c` from karabas-go-firmware lib/common, `net.c` backends). The INI text of config.c lives in page #23 (window 3).
- `src/ts.h`: TS-Conf ports (`__sfr __banked`, #nnAF) and the page map; `src/text.c`: text mode 320x240 = 80 x 30, chars + attributes in page #24 (256 bytes a line, attributes at +128, ink low nibble), font in page #25, palette 15 written through FMAddr.
- Keyboard (`kbd.c` decoder, `kbd_hw.c`): the PS/2 set 2 buffer of the CMOS (#EFF7 = #80, #DFF7 / #BFF7, register #F0 reads the codes); karabas turns it on by register #0C = 1, Unreal / Pentevo by writing 2 to #F0 (done only if #F0 was not empty after the first). Ins toggles РУС / LAT (JCUKEN, CP866).
- Network (`src/net`): ZiFi API v1 of the core (`tr_ts.c`: #C0EF / #C1EF ESP8266, #C2EF / #C3EF RS232 -> MCU -> USB, data #BFEF, command #C7EF, #FF -> reads 1 if present); `at.c` = one TCP link in active mode (+IPD into a 1 KB ring, the transport is read only while the ring has a 64-byte chunk free: the core FIFO + CTS hold the rest), Wi-Fi status / scan / join (+CWAUTOCONN=1) / forget (CWAUTOCONN=0 + CWQAP). `at.c` builds on the host too: `build/at_host` + `tests/test_at_driver.py` run it over a pty against `tools/at_proxy.py` (status, scan, join, 40 KB fetch, refused connection, no forbidden command). unreal-ng has no ZiFi: the network is tried on the board only.
- SD (`sd.c`): Z-controller #77 (0x01 select, 0x03 deselect; bit 0 must stay 1) / #57 (IN = exchange #FF), FatFs R0.15 (ChaN, `src/fatfs`, CP866, LFN, tiny). Settings: `/zifi/gopher.ini` (`config.c`, `ini.c`: comments and order kept, no comments after a value).
- `tools/mkspg.py`: SPG v1.0 writer (uncompressed blocks, <= 16 KB each); `tools/at_proxy.py`: the PC side of the USB serial backend (ESP8266 AT subset, refuses AT+RST / RESTORE / UART_DEF / SYSSTORE).

## Rules

- The ESP8266 is set up by the boot core (fw `app_wifi`: `AT+CWJAP` + `AT+CWAUTOCONN=1`, `AT+UART_DEF=115200,8,1,0,2` = CTS). Never send `AT+RESTORE`, `AT+UART_DEF`, `AT+RST` on detection; a changed baud is `AT+UART_CUR` only; Forget = `AT+CWAUTOCONN=0` + `AT+CWQAP`.
- ZiFi32: the client must work on the firmware already flashed (reflashing the ESP of the Mini G = opening the case). Detect by `GET_VER` + `GET_INFO_STR`, not by `API_VER` alone (the TRACK_* renumbering kept API 1). No undefined command codes (AlexSpawn's 0x28 clashes with another fork). Assume the upstream busy wedge (tslabs/zx-evo#94 open): timeouts + a reset path. Upstream fixes go to our fork `andykarpov/zx-evo` as PRs to tslabs, see [[esp32-zifi32-audit]].
- Tests run in seconds (host unit tests of the parsers, PC AT emulator); emulator runs never on the user display ([[emu-shot]]).
- **Ask before every check on the board** (hwq ticket, capture) and before any ESP / firmware flash.
- One branch per phase of `docs/PLAN.md`, one PR at its end (once the repo is on GitHub).

## Memory

The Claude Code memory of this repo is shared with all karabas-go repos: `~/.claude/projects/<this project>/memory` is a symlink to `../karabas-go-notes` (private repo `andykarpov/karabas-go-notes`). Its `MEMORY.md` is split by repo; commit and push it after every memory change.
