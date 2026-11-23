; vblank_update: copy the first 966 bytes of the buffer at $C100 to VRAM
; starting at the address held in ($C000). The last 58 bytes are NOT copied:
; 1024 * 16 cycles (OUTI) = 16384 > 15960-cycle NTSC VBlank, and the CALL
; itself happens ~387 cycles into VBlank. Fully unrolled to avoid loop overhead.

vblank_update:
    ld hl,($C000)           ; VRAM destination address
    ld a,l
    out ($BF),a             ; address low byte
    ld a,h
    or $40                  ; VRAM write command
    out ($BF),a             ; address high byte + command
    ld hl,$C100             ; source
    ld c,$BE                ; VDP data port
    REPT 966
    outi
    ENDR
    ret
