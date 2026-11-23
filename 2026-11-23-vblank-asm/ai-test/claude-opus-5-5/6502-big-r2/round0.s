; vblank_update - called once per frame from the NMI handler (NTSC).
;
; Order of work:
;   1. OAM DMA from page $02.
;   2. Copy $0300-$039F (160 bytes) to VRAM, starting at the address
;      in ($00 = low, $01 = high). The copy loop is fully unrolled.
;   3. Restore PPUCTRL = $80 and set the scroll to X=0, Y=0.
;
; It uses only A and changes no RAM.

PPUCTRL   = $2000
PPUSTATUS = $2002
PPUSCROLL = $2005
PPUADDR   = $2006
PPUDATA   = $2007
OAMDMA    = $4014

vblank_update:
        ; --- 1. OAM DMA from $0200-$02FF ---
        lda #$02                ; 2
        sta OAMDMA              ; 4 (+513 or 514 cycles of DMA)

        ; --- 2. Set the VRAM address from ($00/$01) ---
        ; PPUCTRL is still $80, so the VRAM increment is 1.
        bit PPUSTATUS           ; 4  reset the $2005/$2006 write latch
        lda $01                 ; 3  high byte first
        sta PPUADDR             ; 4
        lda $00                 ; 3  then low byte
        sta PPUADDR             ; 4

        ; Copy 160 bytes, unrolled: 8 cycles per byte = 1280 cycles.
        .repeat 160, I
        lda $0300+I             ; 4
        sta PPUDATA             ; 4
        .endrepeat

        ; --- 3. Restore the scroll and the nametable select ---
        ; The $2006 writes changed the PPU's internal "t" register.
        ; Two $2005 writes set X and Y to 0, and the $2000 write
        ; sets the nametable bits back to $2000.
        lda #$00                ; 2
        sta PPUSCROLL           ; 4  X scroll = 0
        sta PPUSCROLL           ; 4  Y scroll = 0
        lda #$80                ; 2
        sta PPUCTRL             ; 4  NMI on, nametable $2000, increment 1
        rts
