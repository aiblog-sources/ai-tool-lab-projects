#include "SMSlib.h"

#define LEVEL_W   64
#define VIEW_H    24

#define T_SKY     0
#define T_GRASS   1
#define T_DIRT    2
#define T_BRICK   3

/* Palette: 16 entries, shared by background and sprites
   (the backdrop colour is taken from the sprite palette) */
const unsigned char palette[16] = {
    RGB(0,2,3),  /* 0 sky blue   */
    RGB(0,2,0),  /* 1 grass      */
    RGB(2,1,0),  /* 2 dirt       */
    RGB(1,1,0),  /* 3 dark speck */
    RGB(3,1,0),  /* 4 brick red  */
    RGB(3,3,2),  /* 5 mortar     */
    RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0),
    RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0),
    RGB(0,0,0), RGB(3,3,3)
};

/* 4bpp planar tiles: per row 4 bytes (plane0..plane3) */
const unsigned char tiles[4 * 32] = {
    /* 0: sky (colour 0) */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,

    /* 1: grass top (colour 1) over dirt (colour 2, speckles colour 3) */
    0xFF,0,0,0, 0xFF,0,0,0, 0x00,0xFF,0,0, 0x10,0xFF,0,0,
    0x00,0xFF,0,0, 0x02,0xFF,0,0, 0x00,0xFF,0,0, 0x40,0xFF,0,0,

    /* 2: dirt (colour 2, speckles colour 3) */
    0x00,0xFF,0,0, 0x08,0xFF,0,0, 0x00,0xFF,0,0, 0x00,0xFF,0,0,
    0x20,0xFF,0,0, 0x00,0xFF,0,0, 0x04,0xFF,0,0, 0x00,0xFF,0,0,

    /* 3: brick (colour 4, mortar colour 5) */
    0xFF,0,0xFF,0, 0x08,0,0xFF,0, 0x08,0,0xFF,0, 0x08,0,0xFF,0,
    0xFF,0,0xFF,0, 0x80,0,0xFF,0, 0x80,0,0xFF,0, 0x80,0,0xFF,0
};

/* Ground top row for each of the 64 level columns (wraps seamlessly) */
const unsigned char heights[LEVEL_W] = {
    20,20,20,20,20,20,20,20, 20,20,20,20,20,20,20,20,
    19,19,19,19,18,18,18,18, 18,18,18,18,19,19,19,19,
    20,20,20,20,20,20,20,20, 20,20,20,20,20,20,20,20,
    20,20,20,20,20,20,20,20, 20,20,20,20,20,20,20,20
};

/* Floating brick row per column (0 = none) */
const unsigned char bricks[LEVEL_W] = {
    0,0,0,0,0,0,15,15, 15,15,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 14,14,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 14,14,14,14,0,0,0,0,
    0,0,17,17,0,0,0,0, 0,0,0,0,0,0,0,0
};

unsigned int colbuf[VIEW_H];

static void buildColumn(unsigned char col)
{
    unsigned char r;
    unsigned char top = heights[col];
    unsigned char b = bricks[col];
    for (r = 0; r < VIEW_H; r++) {
        unsigned int t;
        if (r < top)       t = T_SKY;
        else if (r == top) t = T_GRASS;
        else               t = T_DIRT;
        if (b && r == b)   t = T_BRICK;
        colbuf[r] = t;
    }
}

void main(void)
{
    unsigned int pos = 0;   /* scroll position in pixels, 0..511 */
    unsigned char c;

    SMS_displayOff();
    SMS_waitForVBlank();

    SMS_loadBGPalette(palette);
    SMS_loadSpritePalette(palette);
    SMS_setBackdropColor(0);
    SMS_loadTiles(tiles, 0, sizeof(tiles));

    /* initial 32 columns (display is off, VRAM access is safe) */
    for (c = 0; c < 32; c++) {
        buildColumn(c);
        SMS_loadTileMapColumn(c, 0, colbuf, VIEW_H);
    }

    SMS_setBGScrollX(0);
    SMS_setBGScrollY(0);
    /* Blank leftmost 8 pixels: hides the slot being rewritten */
    SMS_VDPturnOnFeature(VDPFEATURE_LEFTCOLBLANK);
    SMS_VDPturnOnFeature(VDPFEATURE_FRAMEIRQ | VDPFEATURE_SHOWDISPLAY);

    for (;;) {
        unsigned char newcol = 0;
        unsigned char slot = 0;
        unsigned char write = ((pos & 7) == 0);

        /* heavy work done outside VBlank */
        if (write) {
            newcol = (unsigned char)(((pos >> 3) + 32) & (LEVEL_W - 1));
            slot   = (unsigned char)((pos >> 3) & 31);
            buildColumn(newcol);
        }

        SMS_waitForVBlank();

        /* VRAM writes only inside VBlank, immediately after it starts */
        if (write) {
            SMS_loadTileMapColumn(slot, 0, colbuf, VIEW_H);
        }
        /* view moves right => scroll register decreases */
        SMS_setBGScrollX((unsigned char)(0 - pos));

        pos = (pos + 1) & 511;
    }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
