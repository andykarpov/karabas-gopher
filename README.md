# karabas gopher

[Русская версия](README.ru.md)

A Gopher browser for TS-Conf (ZX Spectrum), an `.spg` program started from Wild Commander. Made for
the TS-Conf core of the Karabas Go, Mini and Mini G boards, with three ways to the network, a
background music player and a picture viewer.

![The menu of a gopher hole](docs/img/f5-zifi32.jpg)

Status: works on the Karabas Mini G (all features below checked there). Other TS-Conf machines
(ZX-Evolution) are not tested.

## Features

- **Network backends**, found at start, chosen automatically or in the settings:
  - ZiFi32: the ESP32-S3 module of tslabs on the SPI of the Z-controller (firmware 0.6.4 or newer
    for tracker music);
  - ZiFi: an ESP8266 with the standard AT firmware on the UART of the core (set up by the boot core);
  - USB serial: the RS232 of the core through the board MCU (USB CDC) to a PC running
    `tools/at_proxy.py`, or to a real ESP8266 on a USB-UART adapter.
- **Wi-Fi**: network scan, join (hidden networks too), forget, status; the network of ZiFi32 is kept
  in `/zifi/gopher.ini`. An existing `/zifi/ZIFI.INI` of the old ZiFi client is offered as a start.
- **Browsing**: menus, text pages (wrapped at 80 columns), search (type 7), history, bookmarks
  (`/zifi/bookmarks.txt`), any address (F2), home page; UTF-8 / CP1251 / KOI8-R / CP866 pages.
- **Screen**: 80 x 30 text in the font of Wild Commander; the header shows the Wi-Fi network,
  signal, lock and the time of the board clock; a status row, a row of hints, a message row.
- **Downloads**: F10 on any file, into `/downloads/YYYY_MM_DD/`; a taken name gets `_1`.. `_99`;
  Esc cancels and deletes the unfinished file.
- **Music in the background** (it plays on while you browse): Enter on a module plays all the music
  of that menu one after another.
  - PT3 (and TurboSound PT3) on the AY chips, player of S.V.Bulba;
  - MOD on the General Sound of the core (default) or on ZiFi32;
  - XM and S3M on ZiFi32 (firmware 0.6.4+); without it they can only be downloaded.
  - F7 pause, Shift+F7 stop, F8 / Shift+F8 next / previous, from any screen.
- **Pictures**: `.scr` (6912 / 6144), multicolour `.mc` / `.mlt`, gigascreen `.img` (shown without
  flicker), `.bmp` 4 / 8 bits up to 360 x 288. Arrows: the neighbours of the menu, **S**: slide show
  (5 s a picture, the next one loads meanwhile).

## Getting started

1. Build `build/gopher.spg` (below) and copy it to the SD card.
2. In Wild Commander select `gopher.spg` and press Enter.
3. F9 opens the network screen: the modules found, the connection, Wi-Fi, settings (home page,
   charset, downloads folder). F2 there saves `/zifi/gopher.ini`.

| Key | Action |
|---|---|
| Arrows, PgUp / PgDn, Home / End | move |
| Enter / Right | open, play, view |
| BS / Left | back |
| F2 | address |
| F3 | home page |
| F4 / F6 / Del | bookmarks / add one / delete one |
| F5 | reload |
| F9 | network |
| F10 | download (in a text page: save it) |
| F7, Shift+F7, F8, Shift+F8 | pause, stop, next, previous track |
| Ins | Russian / Latin keyboard |

## Building

Needs SDCC 4.2 (`sdcc`, `sdasz80`), sjasmplus (for the PT3 player), Python 3 with Pillow (tests),
`iconv`, gcc and `mkfs.fat` (host tests).

```
make          # build/gopher.spg
make test     # host tests (seconds): keyboard, editor, INI, gopher parsing, AT driver against
              # tools/at_proxy.py, downloads on FatFs (+ fsck.fat), the picture decoders
make emu      # a headless run in the unreal-ng emulator (emu-shot), screenshot build/emu.png
```

The design, the memory map and the phases are in [docs/PLAN.md](docs/PLAN.md) (in Russian), the
state of the work in [docs/BACKLOG.md](docs/BACKLOG.md).

## Third-party code

- FatFs R0.15 by ChaN (`src/fatfs`): its own BSD-style licence in the files.
- The PT3 player of S.V.Bulba (Vortex Tracker II, release 7), from the tslabs zx-evo SDK
  (`src/player/pt3play.asm`), freeware with the author's notice kept.
- The 8x8 font of Wild Commander (`res/font866.bin`, tslabs zx-evo).
- `src/net/esp_at_parse.*` from karabas-go-firmware (MIT).

Everything else: MIT, see [LICENSE](LICENSE).
