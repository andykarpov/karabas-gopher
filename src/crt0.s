; Start-up of the browser (SPG v1.0, run at #C000 with page #20 there).
; Memory map after it: #0000-#1FFF common code, #2000-#3FFF data + stack (page #20 in window 0),
; #4000-#BFFF the current code bank (bank n = pages #30 + 2n, #31 + 2n, see bank.h),
; #C000-#FFFF window 3 for buffers (page #23 at start). 14 MHz + cache, IM 1, interrupts off.
        .module crt0
        .globl  _main
        .globl  _ticks
        .globl  s__DATA, l__DATA, s__INITIALIZED, s__INITIALIZER, l__INITIALIZER

        .area   _HEADER (ABS)
        .org    0
        di                      ; runs at #C000 until the jp: only position-independent code here
        ld      bc, #0x20AF     ; SysConfig: 14 MHz, cache on
        ld      a, #0x06
        out     (c), a
        ld      b, #0x10        ; Page0 = the common page (the page this code is in)
        ld      a, #0x20
        out     (c), a
        ld      b, #0x21        ; MemConfig: W0RAM | W0MAP_N | W0WE
        ld      a, #0x0E
        out     (c), a
        jp      start           ; from here on the copy at #0000 runs

        .org    0x38            ; IM 1: frame counter + the PT3 player
        jp      irq

        .org    0x66            ; NMI: ignore
        retn

        .org    0x80
start:
        ld      sp, #0x4000
        ld      a, #1           ; bank 1 in windows 1 + 2
        call    set_bank
        ld      bc, #0x13AF     ; Page3 = scratch
        ld      a, #0x23
        out     (c), a
        ld      b, #0x2A        ; IntMask: frame only
        ld      a, #0x01
        out     (c), a
        ; zero the data area, then copy the initialised data
        ld      hl, #s__DATA
        ld      bc, #l__DATA
        ld      a, b
        or      c
        jr      z, 2$
        ld      (hl), #0
        dec     bc
        ld      a, b
        or      c
        jr      z, 2$
        ld      d, h
        ld      e, l
        inc     de
        ldir
2$:     ld      bc, #l__INITIALIZER
        ld      a, b
        or      c
        jr      z, 3$
        ld      hl, #s__INITIALIZER
        ld      de, #s__INITIALIZED
        ldir
3$:     ld      a, #1
        ld      (cur_bank), a
        ld      a, #0x23        ; the shadow of Page3 (after the data area was zeroed)
        ld      (_page3), a
        call    gsinit
        im      1
        call    _main
4$:     halt
        jr      4$

; ---- banked calls (SDCC __banked: ld e, #bank / ld hl, #function / call ___sdcc_bcall_ehl;
; the callee finds its arguments past two return addresses and the saved bank byte)
        .area   _CODE
___sdcc_bcall_ehl::
        ld      a, (cur_bank)
        push    af
        inc     sp              ; one byte: the caller's bank
        call    bjump
        dec     sp
        pop     bc              ; B = the caller's bank
        push    af              ; keep A (8-bit results); DE / HL hold the others
        ld      a, b
        call    set_bank
        pop     af
        ret
bjump:  ld      a, e
        call    set_bank
        jp      (hl)

; ---- the frame interrupt: ticks; with pt3_on the player pages #54..#56 go into windows 1..3,
; PLAY runs (#4003), then the bank (cur_bank) and window 3 (page3) are put back. The main code sets
; cur_bank / page3 before the port, so an interrupt in between puts back the new page.
PG_MUSIC = 0x54
irq:
        push    af
        push    hl
        ld      hl, (_ticks)
        inc     hl
        ld      (_ticks), hl
        ld      a, (_pt3_on)
        or      a
        jr      z, 9$
        push    bc
        push    de
        push    ix
        push    iy              ; the PT2 player uses IY
        call    map_music
        call    #0x4003         ; PLAY
        ld      hl, (#0x4009)   ; SETUP bit 7: the end was passed (no loop: muted)
        ld      a, (hl)
        and     #0x80
        jr      z, 1$
        xor     a
        ld      (_pt3_on), a
        inc     a
        ld      (_pt3_end), a
1$:     call    unmap_music
        pop     iy
        pop     ix
        pop     de
        pop     bc
9$:     pop     hl
        pop     af
        ei
        reti

map_music:
        ld      bc, #0x11AF
        ld      a, #PG_MUSIC
        out     (c), a
        inc     b
        inc     a
        out     (c), a
        inc     b
        inc     a
        out     (c), a
        ret

unmap_music:
        ld      a, (cur_bank)
        call    set_bank
        ld      a, (_page3)
        ld      bc, #0x13AF
        out     (c), a
        ret

; void pt3_cmd(uint8_t entry): 0 INIT (the module at #5200, its TurboSound half at pt3_mod2 or 0),
; 6 MUTE; interrupts off meanwhile
_pt3_cmd::
        di
        push    ix
        push    iy
        ld      e, a
        ld      d, #0x40
        call    map_music
        ld      hl, #1$
        push    hl
        push    de
        ld      hl, #0x5200
        ld      de, (_pt3_mod2)
        ret                     ; to #4000 + entry, back to 1$
1$:     call    unmap_music
        pop     iy
        pop     ix
        ei
        ret

; uint8_t gs_put(const uint8_t *p, uint16_t n): HL = p, DE = n; each byte to #B3 after the GS took the
; previous one (status #BB bit 7 clear); A = 0, or 1 if the GS stopped taking bytes (~65536 polls)
_gs_put::
        ld      a, d
        or      e
        ret     z
1$:     ld      bc, #0
2$:     in      a, (#0xBB)
        rla
        jr      nc, 3$
        dec     bc
        ld      a, b
        or      c
        jr      nz, 2$
        inc     a
        ret
3$:     ld      a, (hl)
        out     (#0xB3), a
        inc     hl
        dec     de
        ld      a, d
        or      e
        jr      nz, 1$
        ret

; A = bank: Page1 = #30 + 2A, Page2 = #31 + 2A; keeps BC, DE, HL
set_bank::
        ld      (cur_bank), a
        push    bc
        add     a, a
        add     a, #0x30
        ld      bc, #0x11AF
        out     (c), a
        inc     a
        inc     b
        out     (c), a
        pop     bc
        ret

        ; order of the areas for the linker
        .area   _HOME
        .area   _CODE
        .area   _INITIALIZER
        .area   _GSINIT
        .area   _GSFINAL
        .area   _DATA
        .area   _INITIALIZED
        .area   _BSEG
        .area   _BSS
        .area   _HEAP

        .area   _GSINIT
gsinit::
        .area   _GSFINAL
        ret

        .area   _DATA
_ticks::
        .ds     2
_page3::
        .ds     1
_pt3_on::                       ; 1: the interrupt plays
        .ds     1
_pt3_end::                      ; 1: the module came to its end (set by the interrupt)
        .ds     1
_pt3_mod2::                     ; the second half of a TurboSound module, 0: one AY
        .ds     2
cur_bank:
        .ds     1
