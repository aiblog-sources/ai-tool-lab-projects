```asm
; vblank_update
; Copies 64 bytes from $C100-$C13F to name table row [$C000] (0-23)
; at VRAM $3800 + row*64.
; The handler has already read $BF, so the control port latch is reset.

vblank_update:
        ld      a,($C000)       ; 13  row number, 0-23
        ld      l,a             ; 4
        ld      h,0             ; 7
        add     hl,hl           ; 11  HL = row*2
        add     hl,hl           ; 11  row*4
        add     hl,hl           ; 11  row*8
        add     hl,hl           ; 11  row*16
        add     hl,hl           ; 11  row*32
        add     hl,hl           ; 11  row*64 (max $05C0)
        ld      a,l             ; 4
        out     ($BF),a         ; 11  address low byte
        ld      a,h             ; 4
        or      $78             ; 7   $3800 base | $4000 VRAM write command
        out     ($BF),a         ; 11  address high byte + write command
        ld      hl,$C100        ; 10  source
        ld      bc,$40BE        ; 10  B = 64 bytes, C = VDP data port
        otir                    ; 63*21 + 16 = 1339
        ret                     ; 10
```

VBlank budget: 15960
Cycles used: 1503