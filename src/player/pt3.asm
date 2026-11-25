; The PT3 player of S.V.Bulba (pt3play.asm, Vortex Tracker II r.7, from tslabs zx-evo
; pentevo/sdk/evosdkts/evosdk) assembled by sjasmplus for page PG_MUSIC (#54) at #4000:
; the interrupt of crt0.s maps pages #54..#56 into windows 1..3 and calls PLAY; the module is
; loaded at PT3_MOD (#4900), up to #C000.
        output  "build/pt3.bin"
        org     #4000
        jr      init                    ; +0 HL = the module
        nop
        jp      pt3player.PLAY          ; +3 one frame
        jp      pt3player.MUTE          ; +6
        dw      pt3player.SETUP         ; +9 bit 0: no loop (set below), bit 7: the end was passed
init:   ld      a, 1                    ; play once: the browser goes on to the next track
        ld      (pt3player.SETUP), a
        jp      pt3player.INIT
        include "pt3play.asm"
        assert  $ <= #4900
