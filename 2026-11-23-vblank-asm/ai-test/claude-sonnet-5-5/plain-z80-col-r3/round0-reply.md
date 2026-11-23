```asm
vblank_update:
        ld      a,($C000)
        add     a,a             ; column*2 (0..62)
        ld      e,a
        ld      d,$38           ; DE = $3800 + column*2
        ld      hl,$C100
        ld      b,24
.row:
        ld      a,e
        out     ($BF),a         ; VRAM address low byte
        ld      a,d
        or      $40             ; write-to-VRAM command
        out     ($BF),a         ; VRAM address high byte
        ld      a,(hl)
        inc     hl
        out     ($BE),a         ; entry low byte
        ld      a,(hl)
        inc     hl
        out     ($BE),a         ; entry high byte
        ld      a,e
        add     a,64            ; next row: +64
        ld      e,a
        jr      nc,.next
        inc     d
.next:
        djnz    .row
        ret
```

The routine runs the 24 rows in a loop. Each row sets the VDP write address, then writes the two bytes of its entry. DE holds the VRAM address and advances by 64 per row, with a carry into D when the low byte wraps. The highest address reached is about $3DBE, so ORing $40 into the high byte for the write command is safe. Every address set sends two control-port bytes, so the latch is left clean on return.