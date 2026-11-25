# Backlog

## Открыто

- Следующая Ф1 (клавиатура, строка ввода RUS / ENG, FatFs, INI).
- Проверить: есть ли модуль ESP8266 на Mini G (пины UART в топе есть).
- Проверить: что отдаёт `GOPHER_STREAM_START` в `DATA_SIZE` для gopher (total size).
- Проверить: эмуляция ZiFi / ZiFi32 в unreal-ng (TS-Conf).
- Upstream ZiFi32 (только после tslabs/zx-evo#94 и с согласия): разбор URL по RFC 4266, сохранение сети в NVS по SPI, размер для gopher.
- Позже: автоугадывание кодировки, сборки под другие ядра (.tap / .sna), TLS / gemini — нет.

## Сделано

- 2026-11-25: каталог, CLAUDE.md, PLAN.md (предложение), ссылка памяти на karabas-go-notes.
- 2026-11-25 Ф0: Makefile (SDCC 4.2), crt0 (страницы #20-#23, 14 МГц, IM 1), текстовый режим 80x30 + шрифт CP866 (console-setup FullCyrSlav-VGA8, public domain), упаковщик SPG v1.0 `tools/mkspg.py`, AT-прокси `tools/at_proxy.py` (активный и пассивный приём), host-тесты 6 шт. (0,1 с), `make emu` — «Привет, TS-Conf!» и счётчик кадров в unreal-ng TSL. Mini G 2026-11-25 (hwq #468): запуск из WC 1.11 rc6 с SD2 (`/GOPHER.SPG`), картинка как в эмуляторе, ~50 кадров/с. Лишняя копия `/GOPHER.SPG` осталась и на SD1.
