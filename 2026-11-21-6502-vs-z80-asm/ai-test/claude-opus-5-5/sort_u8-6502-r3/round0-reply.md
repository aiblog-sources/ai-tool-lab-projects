```asm
; sort_u8: in-place ascending heapsort of unsigned bytes
; In : ($00,$01) = pointer to array, X = length (0..255)
; ZP : $02/$03 = ptr+1, $04 = heap end (size), $05 = sift value,
;      $06 = current root, $07 = end-1, $08 = child scratch, $0A = build counter

sort_u8:
        cpx #2
        bcc @rts                ; 0 or 1 element: already sorted
        stx $04                 ; end = n
        dex
        stx $07                 ; end-1
        lda $00                 ; $02/$03 = ptr + 1 (for a[child+1])
        clc
        adc #1
        sta $02
        lda $01
        adc #0
        sta $03

        ; ---- build max-heap: for i = n/2-1 downto 0: sift(i) ----
        lda $04
        lsr
        sta $0A
@build:
        dec $0A
        ldy $0A
        lda ($00),y
        sta $05
        jsr @sift
        lda $0A
        bne @build

        ; ---- sort phase ----
@sortloop:
        ldy $07                 ; Y = last index
        lda ($00),y
        sta $05                 ; val = a[last]
        ldy #0
        lda ($00),y             ; max = a[0]
        ldy $07
        sta ($00),y             ; a[last] = max
        sty $04                 ; heap size = last
        dey
        sty $07                 ; end-1
        ldy #0
        jsr @sift               ; sift val down from root 0
        lda $07
        bne @sortloop
@rts:
        rts

; ---- sift-down: Y = root, $05 = value, $04 = end, $07 = end-1 ----
@sift:
        sty $06                 ; root
        tya
        asl
        bcs @place              ; 2*root >= 256: no children
        ora #1                  ; child = 2*root+1
        cmp $04
        bcs @place              ; child >= end
        tay
        cpy $07
        bcs @one                ; no right child
        lda ($00),y             ; left
        cmp ($02),y             ; vs right
        bcs @have               ; left >= right: A = left
        iny                     ; pick right
@one:
        lda ($00),y
@have:
        cmp $05
        bcc @place              ; a[child] < val: stop
        sty $08
        ldy $06
        sta ($00),y             ; a[root] = a[child]
        ldy $08
        bcs @sift               ; always (C set by cmp)
@place:
        ldy $06
        lda $05
        sta ($00),y
        rts
```

Worst-case cycles: 156670

This is an upper bound for n = 255, counting the JSR and the RTS. It assumes every indirect load crosses a page.