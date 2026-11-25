; The entry table of the AY player blobs written in C (stc.c, asc.c; ay.h), the same as pt3.asm:
; linked first at #4000 of page PG_MUSIC; crt0.s calls +0 INIT (HL = the module at #5200, unused),
; +3 PLAY (one frame) and +6 MUTE from the frame interrupt; +9 = the address of the flags (bit 7:
; the song came back to its loop point). No GSINIT: every variable is set by ay_init.
        .module aycrt
        .globl  _ay_init, _ay_play

        .area   _CODE
        jr      init            ; +0
        nop
        jp      play            ; +3
        jp      mute            ; +6
        .dw     _ay_flags       ; +9

init:   ld      hl, #_ay_r      ; registers, ay_env, ay_flags = 0
        ld      b, #16
1$:     ld      (hl), #0
        inc     hl
        djnz    1$
        jp      _ay_init

play:   xor     a
        ld      (_ay_env), a
        call    _ay_play
; ay_r -> the AY (chip 0 of a TurboSound): registers 0..12, 13 only if ay_env
        ld      bc, #0xFFFD
        ld      a, #0xFF
        out     (c), a
        ld      hl, #_ay_r
        ld      e, #0
2$:     ld      b, #0xFF
        out     (c), e
        ld      b, #0xBF
        ld      a, (hl)
        out     (c), a
        inc     hl
        inc     e
        ld      a, e
        cp      #13
        jr      nz, 2$
        ld      a, (_ay_env)
        or      a
        ret     z
        ld      b, #0xFF
        out     (c), e
        ld      b, #0xBF
        ld      a, (hl)
        out     (c), a
        ret

mute:   ld      bc, #0xFFFD
        ld      a, #0xFF
        out     (c), a
        ld      e, #8
3$:     ld      b, #0xFF
        out     (c), e
        ld      b, #0xBF
        xor     a
        out     (c), a
        inc     e
        ld      a, e
        cp      #11
        jr      nz, 3$
        ret

        .area   _DATA
_ay_r::
        .ds     14
_ay_env::
        .ds     1
_ay_flags::
        .ds     1
