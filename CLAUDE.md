# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

Our own Gopher browser for TS-Conf (`.spg`, Wild Commander / ts-bios), with the features of AlexSpawn's `gopher_esp32.spg` v0.3 (no sources, third party), and one network layer with autodetect and a choice of backend:

- **ZiFi32**: ESP32-S3 with the tslabs firmware (`pentevo/esp32`, SPI `ESP32_SPI`), Mini G only;
- **ZiFi**: ESP8266 with the stock AT firmware on the TS ZiFi UART (#C0EF / #C1EF / #C7EF), 115200, all boards;
- **USB serial**: TS RS232 / EVO RS232 (#C2EF.. / #F8EF..) through the MCU to the clean USB CDC (if02) or a USB-UART adapter.

Docs, in Russian: `docs/PLAN.md` (proposal, decisions, phases), `docs/BACKLOG.md` (open items, keep it current). Follow the phases; record a changed decision in PLAN.md. Status 2026-11-25: plan approved (decisions in PLAN §7: C on SDCC, PC AT proxy + real ESP8266 on a USB-UART adapter, own INI format, no AY backend, repo local only, name open: maybe `karabas-gopher`).

## Rules

- The ESP8266 is set up by the boot core (fw `app_wifi`: `AT+CWJAP` + `AT+CWAUTOCONN=1`, `AT+UART_DEF=115200,8,1,0,2` = CTS). Never send `AT+RESTORE`, `AT+UART_DEF`, `AT+RST` on detection; a changed baud is `AT+UART_CUR` only; Forget = `AT+CWAUTOCONN=0` + `AT+CWQAP`.
- ZiFi32: the client must work on the firmware already flashed (reflashing the ESP of the Mini G = opening the case). Detect by `GET_VER` + `GET_INFO_STR`, not by `API_VER` alone (the TRACK_* renumbering kept API 1). No undefined command codes (AlexSpawn's 0x28 clashes with another fork). Assume the upstream busy wedge (tslabs/zx-evo#94 open): timeouts + a reset path. Upstream fixes go to our fork `andykarpov/zx-evo` as PRs to tslabs, see [[esp32-zifi32-audit]].
- Tests run in seconds (host unit tests of the parsers, PC AT emulator); emulator runs never on the user display ([[emu-shot]]).
- **Ask before every check on the board** (hwq ticket, capture) and before any ESP / firmware flash.
- One branch per phase of `docs/PLAN.md`, one PR at its end (once the repo is on GitHub).

## Memory

The Claude Code memory of this repo is shared with all karabas-go repos: `~/.claude/projects/<this project>/memory` is a symlink to `../karabas-go-notes` (private repo `andykarpov/karabas-go-notes`). Its `MEMORY.md` is split by repo; commit and push it after every memory change.
