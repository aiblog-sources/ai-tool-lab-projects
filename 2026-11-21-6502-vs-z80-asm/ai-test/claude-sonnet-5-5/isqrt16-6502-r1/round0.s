; isqrt16: A = floor(sqrt(n)), n = unsigned 16-bit at $00/$01
; Uses the digit-by-digit (restoring) method, 8 iterations.
; Scratch: $02/$03 = remainder, $04 = root, $05/$06 = trial value

isqrt16_n     = $00
isqrt16_rem   = $02
isqrt16_root  = $04
isqrt16_t     = $05

isqrt16:
        lda #0
        sta isqrt16_rem
        sta isqrt16_rem+1
        sta isqrt16_root
        ldx #8
@loop:
        ; shift the top two bits of n into rem
        asl isqrt16_n
        rol isqrt16_n+1
        rol isqrt16_rem
        rol isqrt16_rem+1
        asl isqrt16_n
        rol isqrt16_n+1
        rol isqrt16_rem
        rol isqrt16_rem+1
        ; t = root*4
        lda isqrt16_root
        asl
        sta isqrt16_t
        lda #0
        rol
        sta isqrt16_t+1
        asl isqrt16_t
        rol isqrt16_t+1
        ; rem - (t+1): with carry clear, SBC subtracts an extra 1
        clc
        lda isqrt16_rem
        sbc isqrt16_t
        tay
        lda isqrt16_rem+1
        sbc isqrt16_t+1
        bcc @no                 ; borrow: rem < trial, carry is clear
        sta isqrt16_rem+1       ; success: carry still set
        sty isqrt16_rem
@no:
        rol isqrt16_root        ; shift in 1 (success) or 0 (fail)
        dex
        bne @loop
        lda isqrt16_root
        rts
