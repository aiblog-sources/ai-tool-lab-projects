; Minimal NROM startup for cc65 C programs (MIT).
; NMI: copies the OAM shadow at $0200 with OAM DMA, writes the PPUCTRL and
; PPUMASK shadows, resets scroll, and records two debug bytes the checker
; reads from RAM: $07F0 = bit 5 of PPUSTATUS (sprite overflow) as read at
; the start of VBlank, $07F1 = NMI count.
; OAM DMA runs only when the program has set OAM_READY ($07F4, see
; oam_ready() in nes.c), so a table still being filled when the NMI arrives is
; never copied half-written. At that DMA the NMI also copies TAG ($07F3, free
; for the program) to SHOWN_TAG ($07F5), so a test can tag each table.
; An NMI that finds no finished table counts a lag frame in LAG ($07F6).

        .export   __STARTUP__ : absolute = 1
        .export   _ppu_ctrl, _ppu_mask
        .import   _main, copydata, zerobss, initlib
        .import   __STACK_START__, __STACK_SIZE__
        .include  "zeropage.inc"

OVF_FLAG  = $07F0
NMI_COUNT = $07F1
TAG       = $07F3
OAM_READY = $07F4
SHOWN_TAG = $07F5
LAG       = $07F6

.segment "ZEROPAGE"
_ppu_ctrl: .res 1
_ppu_mask: .res 1

.segment "HEADER"
        .byte "NES", $1A, 2, 1, $01, $00, 0, 0, 0, 0, 0, 0, 0, 0

.segment "OAM"
        .res 256

.segment "STARTUP"
reset:  sei
        cld
        ldx #$40
        stx $4017
        ldx #$FF
        txs
        inx
        stx $2000
        stx $2001
        stx $4010
        bit $2002
@vb1:   bit $2002
        bpl @vb1
        lda #0
        tax
@clr:   sta $0000,x
        sta $0100,x
        sta $0300,x
        sta $0400,x
        sta $0500,x
        sta $0600,x
        sta $0700,x
        inx
        bne @clr
        lda #$FF                ; every OAM entry off screen (Y = $FF)
@oam:   sta $0200,x
        inx
        bne @oam
@vb2:   bit $2002
        bpl @vb2
        lda #<(__STACK_START__ + __STACK_SIZE__)
        sta c_sp
        lda #>(__STACK_START__ + __STACK_SIZE__)
        sta c_sp+1
        jsr zerobss
        jsr copydata
        jsr initlib
        lda #$1E
        sta _ppu_mask
        lda #$80
        sta _ppu_ctrl
        jmp _main

nmi:    pha
        txa
        pha
        tya
        pha
        lda $2002
        and #$20
        sta OVF_FLAG
        lda OAM_READY
        bne @dma
        inc LAG
        jmp @nodma
@dma:
        lda #0
        sta $2003
        lda #$02
        sta $4014
        lda #0
        sta OAM_READY
        lda TAG
        sta SHOWN_TAG
@nodma:
        lda _ppu_mask
        sta $2001
        lda #0
        sta $2005
        sta $2005
        lda _ppu_ctrl
        sta $2000
        inc NMI_COUNT
        pla
        tay
        pla
        tax
        pla
irq:    rti

.segment "VECTORS"
        .word nmi, reset, irq
