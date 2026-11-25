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

        .org    0x38            ; IM 1: frame counter
        push    af
        push    hl
        ld      hl, (_ticks)
        inc     hl
        ld      (_ticks), hl
        pop     hl
        pop     af
        ei
        reti

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
cur_bank:
        .ds     1
