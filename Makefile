# karabas gopher: TS-Conf .spg (SDCC 4.2, sdasz80) + host tests.
#   make            build/gopher.spg
#   make test       host tests (seconds)
#   make emu        build/emu.png: the spg in unreal-ng TS-Conf, headless (emu-shot)
#   make font       res/font866.bin from the console font (committed)
SDCC    ?= sdcc
SDAS    ?= sdasz80
PY      ?= python3 -I
VERSION := $(shell cat VERSION)
CFLAGS  := -mz80 --std-sdcc11 --opt-code-size -Isrc -Isrc/fatfs -Isrc/net -Isrc/gopher -Ibuild -DAPP_VERSION=\"$(VERSION)\"
LDFLAGS := -mz80 --no-std-crt0 --code-loc 0x0100 --data-loc 0x2000 -Wl-b_BANK1=0x14000 -Wl-b_BANK2=0x24000 -Wl-b_BANK3=0x34000 -Wl-b_BANK4=0x44000
HOSTCC  ?= gcc
SJASM   ?= /opt/sjasmplus/sjasmplus

# sources are UTF-8; they are converted to CP866 (the font of the screen) into build/ first.
# Memory map in src/bank.h: common code in window 0, banks of 32 KB at #4000-#BFFF.
COMMON  := main.c text.c names.c ini.c netplat.c
BANK1   := sd.c config.c bm.c dl.c fatfs/ff.c fatfs/ffunicode.c
BANK2   := settings.c ui.c netui.c browser.c player.c viewer.c kbd.c kbd_hw.c edit.c gopher/url.c gopher/menu.c gopher/cp.c
BANK4   := pic.c gif.c png.c
BANK3   := net/net.c net/gs.c net/at.c net/esp_at_parse.c net/tr_ts.c net/zifi32.c gopher/gopher.c gopher/doc.c
SRCS    := $(COMMON) $(BANK1) $(BANK2) $(BANK3) $(BANK4)
OBJS    := $(addprefix build/,$(SRCS:.c=.rel))
$(addprefix build/,$(BANK1:.c=.rel)): BANKFLAGS = --codeseg BANK1 --constseg BANK1
$(addprefix build/,$(BANK2:.c=.rel)): BANKFLAGS = --codeseg BANK2 --constseg BANK2
$(addprefix build/,$(BANK3:.c=.rel)): BANKFLAGS = --codeseg BANK3 --constseg BANK3
$(addprefix build/,$(BANK4:.c=.rel)): BANKFLAGS = --codeseg BANK4 --constseg BANK4
HOST_TESTS := kbd edit ini
FONT_PSF ?= /usr/share/consolefonts/FullCyrSlav-VGA8.psf.gz

all: build/gopher.spg

build:
	mkdir -p build

build/crt0.rel: src/crt0.s | build
	$(SDAS) -plosgff -o $@ $<

build/%.866.c: src/%.c | build
	@mkdir -p $(dir $@)
	iconv -f UTF-8 -t CP866 $< > $@

build/%.rel: build/%.866.c src/*.h src/fatfs/*.h src/net/*.h src/gopher/*.h Makefile VERSION
	$(SDCC) $(CFLAGS) $(BANKFLAGS) -c $< -o $@

build/gopher.ihx: build/crt0.rel $(OBJS)
	$(SDCC) $(LDFLAGS) $^ -o $@

build/pages.stamp: build/gopher.ihx tools/ihxsplit.py
	$(PY) tools/ihxsplit.py $< build/gopher.map build/pages
	touch $@

# the AY players, each a blob at #4000 with the same entry table (crt0.s calls it in page #54):
# PT3 of S.V.Bulba (two copies: TurboSound), PT2 (his universal PT2 / PT3 player); all in page
# PG_PLAYERS (#5C), player.c copies the one of a track into #54
build/pt3.bin: src/player/pt3.asm src/player/pt3play.asm | build
	sed 's/module pt3player/module pt3playerb/' src/player/pt3play.asm > build/pt3play_b.asm
	$(SJASM) --nologo --msg=war --inc=src/player --inc=build $<
build/pt2.bin: src/player/pt2.asm src/player/ptsplay.asm | build
	$(SJASM) --nologo --msg=war --inc=src/player $<
# STC / ASC: players in C (src/player/ay.h), linked on their own at #4000, variables at #5100
build/player/%.rel: src/player/%.c src/player/ay.h | build
	@mkdir -p build/player
	$(SDCC) -mz80 --std-sdcc11 --opt-code-size -c $< -o $@
build/player/aycrt.rel: src/player/aycrt.s | build
	@mkdir -p build/player
	$(SDAS) -plosgff -o $@ $<
build/%.bin: build/player/aycrt.rel build/player/%.rel tools/ihx2blob.py Makefile
	$(SDCC) -mz80 --no-std-crt0 --code-loc 0x4000 --data-loc 0x5100 build/player/aycrt.rel build/player/$*.rel -o build/player/$*.ihx
	$(PY) tools/ihx2blob.py build/player/$*.ihx build/player/$*.map $@
PLAYERS := build/pt3.bin build/pt2.bin build/stc.bin build/asc.bin
build/players.bin: $(PLAYERS) tools/mkplayers.py
	$(PY) tools/mkplayers.py $@ $(PLAYERS)

# the font and the players go straight into their pages (#25, #5C: src/ts.h)
build/gopher.spg: VERSION build/pages.stamp res/font866.bin build/players.bin tools/mkspg.py
	$(PY) tools/mkspg.py $@ --page 0x20 --desc "karabas gopher $(VERSION)" \
	  $$(for f in build/pages/page*.bin; do p=$${f##*/page}; echo --block 0x$${p%.bin}:$$f; done) \
	  --block 0x25:res/font866.bin --block 0x5C:build/players.bin

# res/font866.bin = the font of Wild Commander (tslabs zx-evo pentevo/soft/WC/source/FONT32L3.CDB, user's
# choice 2026-11-25); `make font-psf` makes the older one from a console font instead
ZXEVO ?= ../zx-evo
font:
	git -C $(ZXEVO) show origin/master:pentevo/soft/WC/source/FONT32L3.CDB > res/font866.bin

font-psf:
	$(PY) tools/mkfont.py $(FONT_PSF) res/font866.bin

build/at_host: tests/host/at_host.c src/net/at.c src/net/esp_at_parse.c src/net/*.h | build
	$(HOSTCC) -std=c11 -Wall -Wextra -O1 -o $@ tests/host/at_host.c src/net/at.c src/net/esp_at_parse.c

test: build/at_host | build
	$(HOSTCC) -std=c11 -Wall -Wextra -O1 -o build/test_gopher tests/host/test_gopher.c src/gopher/url.c src/gopher/menu.c src/gopher/cp.c src/gopher/doc.c && ./build/test_gopher
	@for t in $(HOST_TESTS); do $(HOSTCC) -std=c11 -Wall -Wextra -O1 -o build/test_$$t tests/host/test_$$t.c src/$$t.c \
	  && ./build/test_$$t || exit 1; done
	rm -f build/dl.img && mkfs.fat -F 32 -C build/dl.img 40960 >/dev/null
	iconv -f UTF-8 -t CP866 tests/host/test_dl.c > build/test_dl.866.c
	$(HOSTCC) -std=c11 -Wall -Wextra -O1 -Isrc -Isrc/fatfs -o build/test_dl build/test_dl.866.c src/dl.c src/fatfs/ff.c src/fatfs/ffunicode.c && ./build/test_dl build/dl.img
	fsck.fat -n build/dl.img
	$(HOSTCC) -std=c11 -Wall -Wextra -O1 -DHOST -Itests/host/shim -Isrc -o build/pic_host tests/host/pic_host.c src/pic.c src/gif.c src/png.c
	for p in stc asc; do $(HOSTCC) -std=c11 -Wall -Wextra -O1 -DHOST -o build/ay_$$p tests/host/ay_dump.c src/player/$$p.c || exit 1; done
	$(PY) -m unittest discover -s tests -p 'test_*.py'

emu: build/gopher.spg
	emu-shot unreal --machine TSL --sd $(HOME)/.local/share/emu-shot/tsconf-sd.img --file build/gopher.spg wait:6 shot:build/emu.png

clean:
	rm -rf build

.PHONY: all font font-psf test emu bench clean
.SECONDARY:

# the speed of a picture decoder on the Z80 (tests/emu/picbench.c): make bench BENCH=FILE FMT=n
BENCH_LEN = $(shell stat -c %s $(BENCH))
bench: build/crt0.rel build/text.rel $(addprefix build/,$(BANK4:.c=.rel)) tests/emu/picbench.c | build
	@test -n "$(BENCH)" || { echo "make bench BENCH=FILE FMT=n"; exit 2; }
	iconv -f UTF-8 -t CP866 tests/emu/picbench.c > build/picbench.866.c
	$(SDCC) $(CFLAGS) -DBENCH_FMT=$(FMT) -DBENCH_LEN=$(BENCH_LEN)UL -c build/picbench.866.c -o build/picbench.rel
	$(SDCC) -mz80 --no-std-crt0 --code-loc 0x0100 --data-loc 0x2000 -Wl-b_BANK4=0x44000 build/crt0.rel build/picbench.rel build/text.rel $(addprefix build/,$(BANK4:.c=.rel)) -o build/bench.ihx
	rm -rf build/benchpages && $(PY) tools/ihxsplit.py build/bench.ihx build/bench.map build/benchpages
	rm -rf build/benchfile && mkdir -p build/benchfile && split -b 16384 -d -a 3 $(BENCH) build/benchfile/p
	$(PY) tools/mkspg.py build/bench.spg --page 0x20 --desc bench \
	  $$(for f in build/benchpages/page*.bin; do p=$${f##*/page}; echo --block 0x$${p%.bin}:$$f; done) \
	  --block 0x25:res/font866.bin \
	  $$(i=128; for f in build/benchfile/p*; do printf -- '--block 0x%x:%s ' $$i $$f; i=$$((i+1)); done)
	emu-shot unreal --machine TSL --sd $(HOME)/.local/share/emu-shot/tsconf-sd.img --file build/bench.spg wait:$(or $(WAIT),20) shot:build/bench.png
