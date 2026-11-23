This can't be done with all 256 bytes inside NTSC vblank, so this version keeps the copy correct by blanking the screen and overrunning vblank. Here is why:

- OAM DMA takes 513 or 514 cycles, and NTSC vblank is only about 2273 cycles (20 scanlines × 113.67).
- A PPUDATA write needs a 4-cycle store and a 4-cycle load or pull, so 8 cycles per byte at best. 256 bytes cost 2048 cycles.
- 513 + 2048 is already about 2561, which is more than the 2273 available. The previous version only copied 200 bytes, which is why 56 bytes were wrong.

To get all 256 bytes right, this routine turns rendering off first, so PPU access is legal even after vblank ends. At the end it resets the address and scroll registers and turns rendering back on. The cost is that the copy runs past vblank by about 350 cycles, which blanks the first few scanlines of the frame. That breaks your "finished inside vblank" rule. If you need the rule to hold, the copy has to be split across two or more frames, which means changing the contract of this routine.

```asm
PPUCTRL   = $2000
PPUMASK   = $2001
PPUSTATUS = $2002
PPUSCROLL = $2005
PPUADDR   = $2006
PPUDATA   = $2007
OAMDMA    = $4014

vblank_update:
        lda #$00
        sta PPUMASK             ; forced blank: PPU access is now safe
        lda PPUSTATUS           ; reset the $2005/$2006 write latch

        lda #$02
        sta OAMDMA              ; OAM DMA from $0200 (513/514 cycles)

        lda $01
        sta PPUADDR
        lda $00
        sta PPUADDR             ; destination = ($01:$00)

        ; 8 cycles per byte: LDA abs (4) + STA abs (4)
        .repeat 256, i
        lda $0300+i
        sta PPUDATA
        .endrepeat

        ; Restore: nametable $2000, v = t = 0, fine X = 0, scroll 0,0
        lda #$80
        sta PPUCTRL
        lda #$00
        sta PPUADDR
        sta PPUADDR             ; t = v = $0000
        sta PPUSCROLL
        sta PPUSCROLL           ; X = 0, Y = 0
        lda #$1E
        sta PPUMASK             ; rendering back on
        rts
```

VBlank budget: 2273
Cycles used: 2627