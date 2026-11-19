#include "SMSlib.h"

#define LEVEL_W   64          /* level width in tiles (map is only 32 wide) */
#define SCREEN_H  24          /* visible tile rows */

/* ---------------- Tiles (4bpp planar, 32 bytes each) ---------------- */
/* 0 = sky, 1 = grass-topped ground, 2 = dirt, 3 = brick, 4 = cloud */
#define T_SKY    0
#define T_GRASS  1
#define T_DIRT   2
#define T_BRICK  3
#define T_CLOUD  4

const unsigned char tiles[5 * 32] = {
    /* 0: sky (colour 0) */
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,

    /* 1: grass on dirt (green = 3, dirt = 1, speckle = 2) */
    0xFF,0xFF,0x00,0x00, 0xFF,0xFF,0x00,0x00,
    0xFF,0xFF,0x00,0x00, 0xFF,0x00,0x00,0x00,
    0xFF,0x00,0x00,0x00, 0xEF,0x10,0x00,0x00,
    0xFF,0x00,0x00,0x00, 0xFF,0x00,0x00,0x00,

    /* 2: dirt (colour 1 with colour 2 speckles) */
    0xFF,0x00,0x00,0x00, 0xFF,0x00,0x00,0x00,
    0xF7,0x08,0x00,0x00, 0xFF,0x00,0x00,0x00,
    0xFF,0x00,0x00,0x00, 0xFF,0x00,0x00,0x00,
    0xFD,0x02,0x00,0x00, 0xFF,0x00,0x00,0x00,

    /* 3: brick (red = 5, mortar = 4) */
    0x00,0x00,0xFF,0x00, 0xFE,0x00,0xFF,0x00,
    0xFE,0x00,0xFF,0x00, 0xFE,0x00,0xFF,0x00,
    0x00,0x00,0xFF,0x00, 0xEF,0x00,0xFF,0x00,
    0xEF,0x00,0xFF,0x00, 0xEF,0x00,0xFF,0x00,

    /* 4: cloud (white = 7) */
    0xFF,0xFF,0xFF,0x00, 0xFF,0xFF,0xFF,0x00,
    0xFF,0xFF,0xFF,0x00, 0xFF,0xFF,0xFF,0x00,
    0xFF,0xFF,0xFF,0x00, 0xFF,0xFF,0xFF,0x00,
    0xFF,0xFF,0xFF,0x00, 0xFF,0xFF,0xFF,0x00
};

/* ---------------- Background palette ---------------- */
const unsigned char palette[16] = {
    RGB(0,2,3),   /* 0 sky blue   */
    RGB(2,1,0),   /* 1 brown      */
    RGB(1,1,0),   /* 2 dark brown */
    RGB(0,2,0),   /* 3 green      */
    RGB(2,2,2),   /* 4 mortar     */
    RGB(3,1,0),   /* 5 brick red  */
    RGB(0,0,0),   /* 6 unused     */
    RGB(3,3,3),   /* 7 white      */
    RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0),
    RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0)
};

/* ---------------- Level ---------------- */
/* Tile at (col,row) of the 64-column level. The pattern repeats every
   16 columns, so it wraps seamlessly from column 63 back to column 0. */
static unsigned int level_tile(unsigned char col, unsigned char row)
{
    unsigned char m = col & 15;

    if (row == 22) return T_GRASS;
    if (row == 23) return T_DIRT;

    /* floating brick platforms */
    if (row == 16 && m >= 4 && m <= 7) return T_BRICK;
    /* brick pillars */
    if (m == 12 && (row == 20 || row == 21)) return T_BRICK;
    if (m == 13 && row == 21) return T_BRICK;
    /* clouds */
    if (row == 4 && m >= 9 && m <= 11) return T_CLOUD;
    if (row == 3 && m == 10) return T_CLOUD;

    return T_SKY;
}

static unsigned int colbuf[SCREEN_H];

static void build_column(unsigned char levelcol)
{
    unsigned char r;
    for (r = 0; r < SCREEN_H; r++)
        colbuf[r] = level_tile(levelcol, r);
}

void main(void)
{
    unsigned int cam = 0;      /* camera position in pixels, 0..511 */
    unsigned char c;

    SMS_displayOff();
    SMS_VDPturnOnFeature(VDPFEATURE_HIDEFIRSTCOL);  /* hides the column being rewritten */
    SMS_setSpriteMode(SPRITEMODE_NORMAL);
    SMS_initSprites();
    SMS_copySpritestoSAT();

    SMS_loadBGPalette(palette);
    SMS_setBackdropColor(0);
    SMS_loadTiles(tiles, 0, sizeof(tiles));

    /* fill the whole 32-column tile map with level columns 0..31 */
    for (c = 0; c < 32; c++) {
        build_column(c);
        SMS_loadTileMapColumn(c, 0, colbuf, SCREEN_H);
    }

    SMS_setBGScrollX(0);
    SMS_displayOn();

    /* column 32 goes into map slot 0 (hidden) at cam == 0 */
    build_column(32);

    for (;;) {
        SMS_waitForVBlank();

        /* Increasing cam moves the view right through the level, which
           means the VDP scroll register has to be decreased. */
        SMS_setBGScrollX((unsigned char)(0x100 - (cam & 0xFF)));

        /* Map slot (cam/8)%32 is fully hidden by the blanked first column
           when cam is a multiple of 8: put the next level column there. */
        if ((cam & 7) == 0) {
            SMS_loadTileMapColumn((cam >> 3) & 31, 0, colbuf, SCREEN_H);
        }

        cam = (cam + 1) & (LEVEL_W * 8 - 1);

        /* prepare the column to be written when cam is next a multiple of 8 */
        if ((cam & 7) == 0) {
            build_column(((cam >> 3) + 32) & (LEVEL_W - 1));
        }
    }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
