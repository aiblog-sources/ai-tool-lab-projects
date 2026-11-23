The routine walks down the chosen column of the name table. For each of the 24 rows it sets the VRAM write address on port `$BF`, then sends the entry's two bytes to the data port `$BE`.

- **Address:** each target is `$3800 + column*2 + i*64`, with bit `$4000` added as the VRAM-write flag. So the first address is `$7800 | (column*2)`. Each row after that is 64 bytes further on.
- **Starting state:** the interrupt handler has already reset the control-port latch, so the first two-byte address write is in sync.
- **Speed:** all writes happen during VBlank, so they need no extra delay. They are still spaced 24 or more cycles apart, which also stays within the active-display VRAM access limit.
- **Cost:** about 2,500 T-states, well inside NTSC VBlank.
- **Side effects:** the routine uses no RAM or stack and does not touch any VDP register.

```asm
; vblank_update
; Copies 24 name table entries from RAM $C100-$C12F (2 bytes each, low byte
; first) down column ($C000) of the name table at VRAM $3800:
;   entry i -> VRAM $3800 + i*64 + column*2
; Called from the VBlank handler (control port latch already reset).
; Trashes AF, BC, DE, HL.

vblank_update:
        ld      a,($C000)
        and     31                      ; clamp column to 0..31
        add     a,a                     ; column*2 (0..62)
        ld      e,a
        ld      d,$78                   ; $3800 | $4000 (VRAM write command)
        ld      hl,$C100                ; source entries
        ld      b,24                    ; 24 rows
        ld      c,$BF                   ; VDP control port

.loop:
        out     (c),e                   ; address low byte
        out     (c),d                   ; address high byte | $40 (write)
        ld      a,(hl)
        inc     hl
        out     ($BE),a                 ; entry low byte
        ld      a,(hl)
        inc     hl
        out     ($BE),a                 ; entry high byte

        ld      a,e                     ; next row: address += 64
        add     a,64
        ld      e,a
        jr      nc,.nocarry
        inc     d
.nocarry:
        djnz    .loop
        ret
```