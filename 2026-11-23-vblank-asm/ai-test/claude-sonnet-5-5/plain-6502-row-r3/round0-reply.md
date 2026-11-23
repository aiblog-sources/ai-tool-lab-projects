```asm
; vblank_update: copy 32 bytes from $0300-$031F to the nametable 0 row in $00.
; Clobbers A and X. Uses no RAM.

PPUCTRL   = $2000
PPUSTATUS = $2002
PPUSCROLL = $2005
PPUADDR   = $2006
PPUDATA   = $2007

vblank_update:
        lda PPUSTATUS           ; reset the address latch
        lda $00                 ; row 0-29
        lsr a
        lsr a
        lsr a                   ; row >> 3
        ora #$20                ; high byte = $20 + (row >> 3)
        sta PPUADDR
        lda $00
        asl a
        asl a
        asl a
        asl a
        asl a                   ; low byte = (row & 7) << 5
        sta PPUADDR

        ldx #0
@copy:  lda $0300,x
        sta PPUDATA
        inx
        cpx #32
        bne @copy

        ; Restore the scroll and nametable bits that the $2006 writes clobbered
        lda PPUSTATUS           ; reset the latch
        lda #$80
        sta PPUCTRL             ; NMI on, nametable $2000
        lda #0
        sta PPUSCROLL           ; X scroll = 0
        sta PPUSCROLL           ; Y scroll = 0
        rts
```

The routine takes about 400 cycles, which fits inside NTSC vblank. The row address is `$2000 + row*32`, so the high byte is `$20 + (row >> 3)` and the low byte is `(row & 7) << 5`. The 32 bytes are written to `$2007` with the increment at 1, so they go left to right.

Since rendering is on, the routine finishes by rewriting `$2000` and `$2005` twice. This puts the scroll at 0,0 and the base nametable at `$2000` again, because the `$2006` writes change the PPU's internal scroll address.