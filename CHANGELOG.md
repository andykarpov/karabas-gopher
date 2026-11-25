# Changelog

## 1.1 (2026-11-25)

- ZiFi32: the network of `gopher.ini` is joined at the start even if the module is already up (it
  joins the network saved in its NVS by its console first, e.g. a phone); the header no longer shows
  the name from `gopher.ini` for a network the browser did not join ("сеть модуля").
- Music: PT2 (S.V.Bulba universal player), STC (Sound Tracker) and ASC (ASC Sound Master) on the AY,
  with our own STC / ASC players (frame by frame equal to AYFly); the menu cursor follows the track
  that plays.
- A menu with more than 128 tracks: Enter on a later one plays from it (it played nothing).

## 1.0 (2026-11-25)

The first release, checked on a Karabas Mini G (TS-Conf core).

- Network backends: ZiFi32 (ESP32-S3 on SPI), ZiFi (ESP8266 AT on the UART of the core), USB serial
  (RS232 of the core to a PC with `tools/at_proxy.py` or an ESP8266 on a USB-UART adapter); found at
  start, chosen automatically or in the settings.
- Wi-Fi: scan, join (hidden networks too), forget, status; the ZiFi32 network kept in
  `/zifi/gopher.ini`, the network of the old `/zifi/ZIFI.INI` offered at the first start.
- Browsing: menus, text, search, history, bookmarks, any address, home page; UTF-8 / CP1251 / KOI8-R /
  CP866 pages.
- Screen: 80 x 30 in the font of Wild Commander; header with the network, signal, lock and the time;
  status, hint and message rows.
- Downloads into `/downloads/YYYY_MM_DD/` with the time of the board clock on the files.
- Background music: PT3 and TurboSound PT3 on the AY, MOD on the General Sound (or ZiFi32), XM / S3M on
  ZiFi32 (firmware 0.6.4+); a play list from the menu, F7 / F8 keys from any screen.
- Pictures: SCR, multicolour, gigascreen (without flicker), BMP 4 / 8 bits; slide show (S).
