/* Tiny NES API for the test ROM and the Claude scenes (MIT).
 *
 * OAM shadow: 64 entries x 4 bytes at $0200 (Y, tile, attributes, X), copied
 * to the PPU with OAM DMA in every NMI. A sprite with OAM Y = y is drawn on
 * lines y+1 .. y+8 (y+16 in 8x16 mode). Y = $EF..$FF hides it.
 */
#ifndef NES_H
#define NES_H

#define OAM ((unsigned char *)0x0200)
#define OAM_Y(i)    OAM[((i) << 2) + 0]
#define OAM_TILE(i) OAM[((i) << 2) + 1]
#define OAM_ATTR(i) OAM[((i) << 2) + 2]
#define OAM_X(i)    OAM[((i) << 2) + 3]

#define NMI_COUNT (*(volatile unsigned char *)0x07F1)
#define DEBUG0    (*(volatile unsigned char *)0x07F2)   /* free for the program */
#define TAG       (*(volatile unsigned char *)0x07F3)   /* copied to SHOWN_TAG at each OAM DMA */
#define OAM_READY (*(volatile unsigned char *)0x07F4)
#define SHOWN_TAG (*(volatile unsigned char *)0x07F5)
#define LAG       (*(volatile unsigned char *)0x07F6)   /* NMIs that found no finished table */

/* shadows written to PPUCTRL / PPUMASK in the NMI */
extern unsigned char ppu_ctrl, ppu_mask;
#pragma zpsym ("ppu_ctrl")
#pragma zpsym ("ppu_mask")
#define CTRL_SPR_8X16 0x20

#define PAD_A      0x80
#define PAD_B      0x40
#define PAD_SELECT 0x20
#define PAD_START  0x10
#define PAD_UP     0x08
#define PAD_DOWN   0x04
#define PAD_LEFT   0x02
#define PAD_RIGHT  0x01

void wait_frame(void);                    /* wait for the next NMI */
void oam_ready(void);                     /* the OAM shadow is complete: copy it in the next NMI */
unsigned char pad_read(void);             /* controller 1 */
void ppu_setup(const unsigned char *pal); /* rendering off, 32-byte palette, blank nametable, rendering on */
void oam_hide_all(void);                  /* every byte of OAM to $FF */
unsigned char rnd8(void);                 /* 8-bit xorshift */

#endif
