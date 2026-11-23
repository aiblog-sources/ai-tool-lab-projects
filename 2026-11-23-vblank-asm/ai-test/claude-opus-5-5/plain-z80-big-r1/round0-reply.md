The copy has to fit in VBlank, so it uses unrolled `OUTI` and avoids `OTIR`.

- **Time available:** NTSC VBlank runs from line 193 to line 261, about 69 lines × 228 cycles ≈ 15,700 cycles. The handler's register saving and the `$BF` read come out of that.
- **`OTIR` is too slow:** at 21 cycles per byte, 768 bytes take about 16,100 cycles.
- **Unrolled `OUTI` fits:** at 16 cycles per byte, 768 bytes take 12,288 cycles. The loop runs 6 times over 128 `OUTI`s, adding about 90 cycles, so the routine totals about 12,450 cycles.
- **Write spacing:** writes 16 cycles apart are safe during VBlank, when the VDP has free access to VRAM.
- **Address range:** the start address is at most `$3500`, so the copy ends no later than `$37FF` and stays inside VRAM. ORing `$40` into the high byte makes it a VRAM-write command.

```asm
; vblank_update
; Copies 768 bytes from RAM $C100-$C3FF to VRAM starting at the
; address stored (little-endian) at $C000/$C001.
; Must run inside VBlank: uses unrolled OUTI (16 cycles/byte),
; total ~12.5k cycles, which fits in the NTSC vertical blank.

vblank_update:
        ; --- set VRAM write address ---
        ld      hl,($C000)          ; L = low byte, H = high byte
        ld      a,l
        out     ($BF),a             ; address low byte
        ld      a,h
        or      $40                 ; command: VRAM write
        out     ($BF),a             ; address high byte + command

        ; --- copy 768 bytes: 6 x 128 unrolled OUTI ---
        ld      hl,$C100            ; source
        ld      c,$BE               ; VDP data port
        ld      d,6                 ; 6 blocks of 128 bytes
.loop:
        REPT 128
        outi
        ENDR
        dec     d
        jp      nz,.loop
        ret
```