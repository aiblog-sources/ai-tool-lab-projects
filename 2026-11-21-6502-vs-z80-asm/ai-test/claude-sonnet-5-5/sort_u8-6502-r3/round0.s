; sort_u8: insertion sort, unsigned bytes, ascending, in place.
; In:  ($00) = array pointer, X = length (0..255)
; Scratch: $02 = length, $03 = key
sort_u8:
        stx $02
        cpx #2
        bcc @done           ; length < 2: nothing to do
        ldx #1              ; X = i
@outer:
        txa
        tay                 ; Y = j = i
        lda ($00),y
        sta $03             ; key = a[i]
@inner:
        dey                 ; Y = j-1
        lda $03
        cmp ($00),y         ; key >= a[j-1] ?
        bcs @ins1           ; yes: insert at j
        lda ($00),y         ; shift a[j-1] up to a[j]
        iny
        sta ($00),y
        dey
        bne @inner
        beq @place          ; Y = 0: insert at index 0
@ins1:
        iny
@place:
        lda $03
        sta ($00),y
        inx
        cpx $02
        bne @outer
@done:
        rts
