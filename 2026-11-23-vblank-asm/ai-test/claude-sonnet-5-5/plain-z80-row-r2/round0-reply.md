```asm
vblank_update:
        ld      a,(0C000h)          ; row number 0..23
        ld      l,a
        ld      h,0
        add     hl,hl               ; row*2
        add     hl,hl               ; row*4
        add     hl,hl               ; row*8
        add     hl,hl               ; row*16
        add     hl,hl               ; row*32
        add     hl,hl               ; row*64
        ld      de,3800h
        add     hl,de               ; HL = VRAM address of the row

        ld      a,l
        out     (0BFh),a            ; address low byte (latch already reset)
        ld      a,h
        or      40h                 ; command: VRAM write
        out     (0BFh),a            ; address high byte

        ld      hl,0C100h           ; source buffer
        ld      c,0BEh              ; VDP data port
        ld      b,64                ; 32 entries * 2 bytes
        otir
        ret
```

The routine multiplies the row by 64 and adds $3800 to get the row's VRAM address. It sets that address on the control port as a VRAM write, then copies the 64 bytes to the data port with OTIR. It doesn't change any VDP registers or touch RAM other than reading $C000 and the $C100 buffer.