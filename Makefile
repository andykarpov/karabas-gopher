# karabas gopher: TS-Conf .spg (SDCC 4.2, sdasz80) + host tests.
#   make            build/gopher.spg
#   make test       host tests (seconds)
#   make emu        build/emu.png: the spg in unreal-ng TS-Conf, headless (emu-shot)
#   make font       res/font866.bin from the console font (committed)
SDCC    ?= sdcc
SDAS    ?= sdasz80
PY      ?= python3 -I
CFLAGS  := -mz80 --std-sdcc11 --opt-code-size -Isrc -Isrc/fatfs -Ibuild
LDFLAGS := -mz80 --no-std-crt0 --code-loc 0x0100 --data-loc 0x2000 -Wl-b_BANK1=0x14000 -Wl-b_BANK2=0x24000
HOSTCC  ?= gcc

# sources are UTF-8; they are converted to CP866 (the font of the screen) into build/ first.
# Memory map in src/bank.h: common code in window 0, banks of 32 KB at #4000-#BFFF.
COMMON  := main.c text.c kbd.c kbd_hw.c edit.c names.c ini.c
BANK1   := sd.c config.c fatfs/ff.c fatfs/ffunicode.c
BANK2   := settings.c
SRCS    := $(COMMON) $(BANK1) $(BANK2)
OBJS    := $(addprefix build/,$(SRCS:.c=.rel))
$(addprefix build/,$(BANK1:.c=.rel)): BANKFLAGS = --codeseg BANK1 --constseg BANK1
$(addprefix build/,$(BANK2:.c=.rel)): BANKFLAGS = --codeseg BANK2 --constseg BANK2
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

build/%.rel: build/%.866.c src/*.h src/fatfs/*.h Makefile
	$(SDCC) $(CFLAGS) $(BANKFLAGS) -c $< -o $@

build/gopher.ihx: build/crt0.rel $(OBJS)
	$(SDCC) $(LDFLAGS) $^ -o $@

build/pages.stamp: build/gopher.ihx tools/ihxsplit.py
	$(PY) tools/ihxsplit.py $< build/gopher.map build/pages
	touch $@

# the font goes straight into its page (#25, src/ts.h)
build/gopher.spg: build/pages.stamp res/font866.bin tools/mkspg.py
	$(PY) tools/mkspg.py $@ --page 0x20 --desc "karabas gopher" \
	  $$(for f in build/pages/page*.bin; do p=$${f##*/page}; echo --block 0x$${p%.bin}:$$f; done) \
	  --block 0x25:res/font866.bin

font:
	$(PY) tools/mkfont.py $(FONT_PSF) res/font866.bin

test: | build
	@for t in $(HOST_TESTS); do $(HOSTCC) -std=c11 -Wall -Wextra -O1 -o build/test_$$t tests/host/test_$$t.c src/$$t.c \
	  && ./build/test_$$t || exit 1; done
	$(PY) -m unittest discover -s tests -p 'test_*.py'

emu: build/gopher.spg
	emu-shot unreal --machine TSL --sd $(HOME)/.local/share/emu-shot/tsconf-sd.img --file build/gopher.spg wait:6 shot:build/emu.png

clean:
	rm -rf build

.PHONY: all font test emu clean
.SECONDARY:
