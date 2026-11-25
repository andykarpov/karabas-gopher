# karabas gopher: TS-Conf .spg (SDCC 4.2, sdasz80) + host tests.
#   make            build/gopher.spg
#   make test       host tests (seconds)
#   make emu        build/emu.png: the spg in unreal-ng TS-Conf, headless (emu-shot)
#   make font       res/font866.bin from the console font (committed)
SDCC    ?= sdcc
SDAS    ?= sdasz80
PY      ?= python3 -I
CFLAGS  := -mz80 --std-sdcc11 --opt-code-size -Isrc -Ibuild
LDFLAGS := -mz80 --no-std-crt0 --code-loc 0x0100 --data-loc 0x8000

SRCS    := src/main.c src/text.c
OBJS    := $(SRCS:src/%.c=build/%.rel)
FONT_PSF ?= /usr/share/consolefonts/FullCyrSlav-VGA8.psf.gz

all: build/gopher.spg

build:
	mkdir -p build

build/font866.h: res/font866.bin tools/bin2c.py | build
	$(PY) tools/bin2c.py $< $@ font866

build/crt0.rel: src/crt0.s | build
	$(SDAS) -plosgff -o $@ $<

build/text.rel: build/font866.h
build/%.rel: src/%.c src/*.h | build
	$(SDCC) $(CFLAGS) -c $< -o $@

build/gopher.ihx: build/crt0.rel $(OBJS)
	$(SDCC) $(LDFLAGS) $^ -o $@

build/gopher.bin: build/gopher.ihx
	makebin -p -s 65536 $< $@
	@n=$$(stat -c %s $@); echo "code $$n bytes of 32512"; [ $$n -le 32768 ] || { echo "code over #8000"; rm -f $@; exit 1; }

build/gopher.spg: build/gopher.bin tools/mkspg.py
	head -c 16384 $< > build/code0.bin
	tail -c +16385 $< > build/code1.bin
	$(PY) tools/mkspg.py $@ --page 0x20 --desc "karabas gopher" --block 0x20:build/code0.bin \
	  $$( [ -s build/code1.bin ] && echo --block 0x21:build/code1.bin )

font:
	$(PY) tools/mkfont.py $(FONT_PSF) res/font866.bin

test:
	$(PY) -m unittest discover -s tests -p 'test_*.py'

emu: build/gopher.spg
	emu-shot unreal --machine TSL --sd $(HOME)/.local/share/emu-shot/tsconf-sd.img --file build/gopher.spg wait:6 shot:build/emu.png

clean:
	rm -rf build

.PHONY: all font test emu clean
