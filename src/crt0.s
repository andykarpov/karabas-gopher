; Start-up of the browser (SPG v1.0, run at #C000 with page PG_CODE0 there).
; Memory map after it: #0000-#7FFF code (pages #20, #21), #8000-#BFFF data + stack (page #22),
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
        ld      b, #0x10        ; Page0 = code 0 (the page this code is in)
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
        ld      b, #0x11        ; Page1 = code 1
        ld      a, #0x21
        out     (c), a
        ld      b, #0x12        ; Page2 = data
        inc     a
        out     (c), a
        ld      b, #0x13        ; Page3 = scratch
        inc     a
        out     (c), a
        ld      b, #0x2A        ; IntMask: frame only
        ld      a, #0x01
        out     (c), a
        ld      sp, #0xC000
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
3$:     call    gsinit
        im      1
        call    _main
4$:     halt
        jr      4$

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
