The routine fits in vblank. It uses about 1,870 of the roughly 2,270 CPU cycles available on NTSC, counting the NMI entry and register saves. Two choices make that work:

- **Fully unrolled copy:** each byte costs 8 cycles (`lda abs` plus `sta $2007`), so 160 bytes take 1,280 cycles. A plain loop would take about 15 cycles per byte (2,400 total) and would run past the end of vblank.
- **Scroll reset at the end:** writing `$2006` overwrites the PPU's internal scroll register. So the routine writes `PPUCTRL` and `PPUSCROLL` again before returning, keeping the display on nametable `$2000` with no scroll.

```asm
; vblank_update
; Called from NMI (NTSC) with rendering on.
; 1) OAM DMA from page $02
; 2) Copy $0300-$039F (160 bytes) to PPU address in ($00 lo, $01 hi)
; 3) Restore PPUCTRL and scroll (0,0, nametable $2000)
;
; Cycle estimate (from the NMI going off):
;   NMI entry + register saves + JSR  ~  26
;   OAM DMA setup + DMA               ~ 526
;   PPUADDR setup                     ~  18
;   Unrolled copy, 160 * 8            = 1280
;   PPUCTRL / PPUSCROLL reset         ~  16
;   Total                             ~ 1866  (< ~2273 vblank cycles)

vblank_update:
        ; --- OAM DMA ---
        lda #$00
        sta $2003               ; OAMADDR = 0
        lda #$02
        sta $4014               ; DMA from $0200-$02FF

        ; --- Set PPU address ---
        bit $2002               ; reset the $2005/$2006 write latch
        lda $01
        sta $2006               ; high byte
        lda $00
        sta $2006               ; low byte

        ; --- Copy 160 bytes, fully unrolled (8 cycles per byte) ---
        .repeat 160, I
        lda $0300+I
        sta $2007
        .endrepeat

        ; --- Restore PPUCTRL and scroll ---
        ; Writing $2006 overwrote the PPU's internal scroll register.
        lda #$80
        sta $2000               ; NMI on, nametable $2000, increment 1
        lda #$00
        sta $2005               ; X scroll = 0
        sta $2005               ; Y scroll = 0

        rts
```