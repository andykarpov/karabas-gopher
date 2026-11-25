; Two copies of the PT3 player of S.V.Bulba (pt3play.asm, Vortex Tracker II r.7, from tslabs zx-evo
; pentevo/sdk/evosdkts/evosdk; the second copy is the same file with its module renamed, made by the
; Makefile) for page PG_MUSIC (#54) at #4000: a TurboSound module ("02TS" at its end) plays its
; first half on chip 0 and the second on chip 1. The interrupt of crt0.s maps pages #54..#56 into
; windows 1..3 and calls PLAY; the module is at PT3_MOD (#5200) up to #FFFF.
        output  "build/pt3.bin"
        org     #4000
        jr      init                    ; +0 HL = the module, DE = its second half (0: one AY)
        nop
        jp      play                    ; +3 one frame
        jp      mute                    ; +6
        dw      pt3player.SETUP         ; +9 bit 7: the end was passed (of the first half)

init:   ld      a, 1                    ; play once: the browser goes on to the next track
        ld      (pt3player.SETUP), a
        ld      (pt3playerb.SETUP), a
        ld      a, d
        or      e
        ld      (ts), a
        jr      z, 1f
        push    hl
        ex      de, hl
        call    chip1
        call    pt3playerb.INIT
        pop     hl
1:      call    chip0
        jp      pt3player.INIT

play:   call    chip0
        ifndef  TS_TEST                 ; TS_TEST: the first half is silent (a check of chip 1)
        call    pt3player.PLAY
        else
        call    pt3player.MUTE
        endif
        ld      a, (ts)
        or      a
        ret     z
        call    chip1
        call    pt3playerb.PLAY
        jr      chip0

mute:   call    chip1
        call    pt3playerb.MUTE
        call    chip0
        jp      pt3player.MUTE

chip0:  ld      a, #FF                  ; TurboSound: #FF = chip 0, #FE = chip 1
        jr      2f
chip1:  ld      a, #FE
2:      ld      bc, #FFFD
        out     (c), a
        ret

ts:     db      0

        include "pt3play.asm"
        include "pt3play_b.asm"
        assert  $ <= #5200
