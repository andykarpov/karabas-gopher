# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

Our own Gopher browser for TS-Conf (`.spg`, Wild Commander / ts-bios), with the features of AlexSpawn's `gopher_esp32.spg` v0.3 (no sources, third party), and one network layer with autodetect and a choice of backend:

- **ZiFi32**: ESP32-S3 with the tslabs firmware (`pentevo/esp32`, SPI `ESP32_SPI`), Mini G only;
- **ZiFi**: ESP8266 with the stock AT firmware on the TS ZiFi UART (#C0EF / #C1EF / #C7EF), 115200, all boards;
- **USB serial**: TS RS232 / EVO RS232 (#C2EF.. / #F8EF..) through the MCU to the clean USB CDC (if02) or a USB-UART adapter.

Docs, in Russian: `docs/PLAN.md` (proposal, decisions, phases), `docs/BACKLOG.md` (open items, keep it current). Follow the phases; record a changed decision in PLAN.md. Status 2026-11-25: plan approved (decisions in PLAN §7: C on SDCC, PC AT proxy + real ESP8266 on a USB-UART adapter, own INI format, no AY backend, repo local only, name open: maybe `karabas-gopher`).

## Build and layout

- `make` -> `build/gopher.spg` (SDCC 4.2 `sdcc` / `sdasz80`, `makebin`, Python 3 tools; default sdcccall, no prebuilt tslabs SDK libs). `make test` = host tests (`tests/`, seconds). `make emu` = the spg in unreal-ng TS-Conf, headless via emu-shot -> `build/emu.png` (read the PNG). `make font` regenerates `res/font866.bin` (CP866 8x8 from the public-domain console font FullCyrSlav-VGA8).
- `src/crt0.s`: SPG runs at #C000 in page #20, maps code pages #20/#21 at #0000-#7FFF, data + stack page #22 at #8000-#BFFF, window 3 = paged buffers (page #23 at start); 14 MHz + cache, IM 1 frame counter `ticks`. Code limit 32 KB (the Makefile checks).
- `src/ts.h`: TS-Conf ports (`__sfr __banked`, #nnAF) and the page map; `src/text.c`: text mode 320x240 = 80 x 30, chars + attributes in page #24 (256 bytes a line, attributes at +128, ink low nibble), font in page #25, palette 15 written through FMAddr.
- `tools/mkspg.py`: SPG v1.0 writer (uncompressed blocks, <= 16 KB each); `tools/at_proxy.py`: the PC side of the USB serial backend (ESP8266 AT subset, refuses AT+RST / RESTORE / UART_DEF / SYSSTORE).

## Rules

- The ESP8266 is set up by the boot core (fw `app_wifi`: `AT+CWJAP` + `AT+CWAUTOCONN=1`, `AT+UART_DEF=115200,8,1,0,2` = CTS). Never send `AT+RESTORE`, `AT+UART_DEF`, `AT+RST` on detection; a changed baud is `AT+UART_CUR` only; Forget = `AT+CWAUTOCONN=0` + `AT+CWQAP`.
- ZiFi32: the client must work on the firmware already flashed (reflashing the ESP of the Mini G = opening the case). Detect by `GET_VER` + `GET_INFO_STR`, not by `API_VER` alone (the TRACK_* renumbering kept API 1). No undefined command codes (AlexSpawn's 0x28 clashes with another fork). Assume the upstream busy wedge (tslabs/zx-evo#94 open): timeouts + a reset path. Upstream fixes go to our fork `andykarpov/zx-evo` as PRs to tslabs, see [[esp32-zifi32-audit]].
- Tests run in seconds (host unit tests of the parsers, PC AT emulator); emulator runs never on the user display ([[emu-shot]]).
- **Ask before every check on the board** (hwq ticket, capture) and before any ESP / firmware flash.
- One branch per phase of `docs/PLAN.md`, one PR at its end (once the repo is on GitHub).

## Memory

The Claude Code memory of this repo is shared with all karabas-go repos: `~/.claude/projects/<this project>/memory` is a symlink to `../karabas-go-notes` (private repo `andykarpov/karabas-go-notes`). Its `MEMORY.md` is split by repo; commit and push it after every memory change.
