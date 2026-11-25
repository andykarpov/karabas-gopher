; PT2 (Pro Tracker 2) player blob for page PG_MUSIC (#54) at #4000: the universal PT2 / PT3 player of
; S.V.Bulba (ptsplay.asm) in its PT2 mode, one AY (chip 0). The same entry table as pt3.asm (crt0.s
; and player.c use it); the module is at PT3_MOD (#5200).
        output  "build/pt2.bin"
        org     #4000
        jr      init                    ; +0 HL = the module (DE ignored)
        nop
        jp      play                    ; +3 one frame
        jp      mute                    ; +6
        dw      SETUP                   ; +9 bit 7: the end (loop point) was passed

init:   ld      a, 3                    ; bit 1 PT2, bit 0 no loop (muted at the loop point)
        ld      (SETUP), a
        call    chip0
        ld      de, 0
        jp      START + 3

play:   call    chip0
        jp      START + 5

mute:   call    chip0
        jp      START + 8

chip0:  ld      a, #FF                  ; TurboSound: #FF = chip 0
        ld      bc, #FFFD
        out     (c), a
        ret

        include "ptsplay.asm"
        assert  $ <= #5200
