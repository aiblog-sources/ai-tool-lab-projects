; vblank_update: copy the first 928 bytes of the 1024-byte buffer at $C100
; to VRAM starting at the address held in ($C000). The last 96 bytes are NOT copied.
;
; The full 1024 bytes cannot be copied in one NTSC VBlank. The fastest
; possible VDP data write is OUTI at 16 cycles per byte, so 1024 bytes need
; 16384 cycles. NTSC VBlank is only 70 lines * 228 = 15960 cycles, and that
; has to cover the handler's own overhead as well. I copy the most that
; safely fits, leaving a margin for the handler's register saves and
; interrupt latency.

vblank_update:
    ld hl,($C000)           ; VRAM destination address
    ld a,l
    out ($BF),a             ; address low byte
    ld a,h
    or $40                  ; VRAM write command
    out ($BF),a             ; address high byte + command
    ld hl,$C100             ; source
    ld c,$BE                ; VDP data port
    ld d,29                 ; 29 blocks * 32 bytes = 928 bytes
.vblank_update_blk:
    REPT 32
    outi
    ENDR
    dec d
    jp nz,.vblank_update_blk
    ret
