; sort_u8: insertion sort, ascending, unsigned bytes
; In:  ($00/$01) = array pointer, X = length (0..255)
; Scratch: $02 = length, $03 = key
sort_u8:
        cpx #2
        bcc @done           ; 0 or 1 elements: nothing to do
        stx $02             ; n
        ldx #1              ; i = 1
@outer:
        txa
        tay                 ; Y = j = i
        lda ($00),y
        sta $03             ; key = a[i]
@inner:
        dey                 ; Y = j-1
        lda $03
        cmp ($00),y         ; key - a[j-1]
        bcs @ins1           ; key >= a[j-1]: insert at j
        lda ($00),y         ; shift a[j-1] up to a[j]
        iny
        sta ($00),y
        dey
        bne @inner
        beq @store          ; Y = 0: insert at index 0 (always taken)
@ins1:
        iny
@store:
        lda $03
        sta ($00),y
        inx
        cpx $02
        bne @outer
@done:
        rts
