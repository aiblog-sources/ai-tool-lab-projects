The routine sets the VRAM write address from `$C000`/`$C001`, then copies the 768 bytes with unrolled `OUTI` (6 passes of 128).

**Timing:**
- `OUTI` takes 16 cycles per byte. `OTIR` takes 21, which would be about 16,100 cycles in total and would overrun VBlank.
- The whole copy takes about 12,400 cycles.
- The NTSC VBlank window after the frame interrupt is about 69 lines × 228 = about 15,700 cycles.
- That leaves roughly 3,300 cycles for the handler's register saves and interrupt overhead.
- During VBlank the VDP accepts writes at `OUTI` speed. The code assumes the copy is all done inside VBlank.

**RAM:** It uses no RAM except the stack slot for the return address, so it never touches `$C010`–`$C01F`.

```asm
; vblank_update
; Copies 768 bytes from RAM $C100-$C3FF to VRAM, starting at the
; address stored at $C000 (low) / $C001 (high).
; Must run inside VBlank: unrolled OUTI (16 cycles/byte), about 12.4k cycles total.
; The control port latch has already been reset by the caller's read of $BF.

vblank_update:
        ld      hl,($C000)          ; HL = destination VRAM address
        ld      a,l
        out     ($BF),a             ; address low byte
        ld      a,h
        or      $40                 ; command 01 = VRAM write
        out     ($BF),a             ; address high byte + command

        ld      hl,$C100            ; source
        ld      c,$BE               ; VDP data port
        ld      d,6                 ; 6 passes x 128 bytes = 768

.loop:
        DUP 128
        outi                        ; (HL) -> port C, HL++, B-- (B unused)
        EDUP
        dec     d
        jp      nz,.loop
        ret
```