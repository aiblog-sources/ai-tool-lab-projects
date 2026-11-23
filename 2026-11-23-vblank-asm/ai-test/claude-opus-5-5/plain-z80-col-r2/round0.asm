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
