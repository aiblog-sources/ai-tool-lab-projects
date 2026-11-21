; sort_u8 - in-place ascending heapsort of unsigned bytes
; In:  ($00/$01) = pointer to the array, X = length (0..255)
; Uses zero page $02-$08 as scratch.
;
; The heap is indexed from 1 through the base pointer B = ptr-1.
; This way a[i] is (B),Y with Y=i, and the children of i are 2i and 2i+1.

sort_u8_B   = $02           ; $02/$03 = ptr - 1
sort_u8_N   = $04           ; current heap size
sort_u8_I   = $05           ; build-phase index (0 = extraction phase)
sort_u8_V   = $06           ; value being sifted down
sort_u8_H   = $07           ; hole index
sort_u8_T   = $08           ; temp

sort_u8:
        cpx #2
        bcc @ret            ; 0 or 1 element: nothing to do
        stx sort_u8_N
        lda $00             ; carry is set here (X >= 2)
        sbc #1
        sta sort_u8_B
        lda $01
        sbc #0
        sta sort_u8_B+1
        txa
        lsr
        sta sort_u8_I       ; I = n/2 (>= 1)

; ---- build heap: sift down nodes I = n/2 .. 1 ----
@build_next:
        ldy sort_u8_I
        lda (sort_u8_B),y
        sta sort_u8_V
        sty sort_u8_H
        jmp @sift

; ---- extraction loop check ----
@ext_chk:
        ldx sort_u8_N
        cpx #2
        bcs @extract
@ret:
        rts

; ---- extraction: swap a[1] and a[N], N--, sift down the root ----
@extract:
        ldy sort_u8_N
        lda (sort_u8_B),y   ; v = a[N]
        sta sort_u8_V
        ldy #1
        lda (sort_u8_B),y   ; a[N] = a[1]
        ldy sort_u8_N
        sta (sort_u8_B),y
        dec sort_u8_N
        ldy #1
        sty sort_u8_H       ; hole = 1, falls into the sift

; ---- sift down: Y = H = hole, V = value, N = heap size ----
@sift:
        tya
        asl                 ; c = 2*hole
        bcs @done           ; overflow: no children
        cmp sort_u8_N
        bcs @maybe_one      ; c >= N
        tay                 ; two children: c and c+1
        lda (sort_u8_B),y
        iny
        cmp (sort_u8_B),y   ; a[c] >= a[c+1] ?
        bcc @right
        dey                 ; take the left child (A = a[c])
        cmp sort_u8_V
        bcc @done           ; child < V: stop
        sty sort_u8_T
        ldy sort_u8_H
        sta (sort_u8_B),y   ; a[hole] = child
        ldy sort_u8_T
        sty sort_u8_H       ; hole = child
        jmp @sift

@right:
        lda (sort_u8_B),y   ; A = a[c+1], Y = c+1
        cmp sort_u8_V
        bcc @done
        sty sort_u8_T
        ldy sort_u8_H
        sta (sort_u8_B),y
        ldy sort_u8_T
        sty sort_u8_H
        jmp @sift

@maybe_one:
        bne @done           ; c > N: no children
        tay                 ; c == N: exactly one child
        lda (sort_u8_B),y
        cmp sort_u8_V
        bcc @done
        sty sort_u8_T
        ldy sort_u8_H
        sta (sort_u8_B),y
        ldy sort_u8_T
        sty sort_u8_H       ; that child is a leaf, fall into done

@done:
        ldy sort_u8_H
        lda sort_u8_V
        sta (sort_u8_B),y   ; a[hole] = V
        ldx sort_u8_I
        beq @ext_chk        ; extraction phase
        dex
        stx sort_u8_I
        beq @extract        ; build finished, start extraction
        jmp @build_next
