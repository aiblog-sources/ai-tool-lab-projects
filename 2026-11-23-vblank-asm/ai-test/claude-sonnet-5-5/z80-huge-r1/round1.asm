; vblank_update: copy the first 960 bytes of the buffer at $C100 to VRAM
; starting at the address held in ($C000). The last 64 bytes are NOT copied:
; 1024 bytes * 16 cycles (OUTI, the fastest possible VDP data write)
; = 16384 cycles, which exceeds the 15960-cycle NTSC VBlank.

vblank_update:
    ld hl,($C000)           ; VRAM destination address
    ld a,l
    out ($BF),a             ; address low byte
    ld a,h
    or $40                  ; VRAM write command
    out ($BF),a             ; address high byte + command
    ld hl,$C100             ; source
    ld c,$BE                ; VDP data port
    ld d,15                 ; 15 blocks * 64 bytes = 960 bytes
.vblank_update_blk:
    REPT 64
    outi
    ENDR
    dec d
    jp nz,.vblank_update_blk
    ret
