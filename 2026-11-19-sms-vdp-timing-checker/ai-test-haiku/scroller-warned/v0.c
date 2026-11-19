/*
 * Horizontal scroller for the Sega Master System (devkitSMS / SMSlib, SDCC).
 *
 * Level: 64 tiles (512 px) wide, 24 rows high. The 32-column tilemap is
 * refilled one column at a time, one tile column ahead of the screen.
 * Scrolling is 1 px per frame, and the level stops at its right edge.
 *
 * Build: sdcc -c -mz80 main.c  (link with crt0_sms.rel and SMSlib.lib, then makesms)
 */

#include <SMSlib.h>

/* ------------------------------------------------------------------------ */
/* Tile graphics: 4bpp planar 8x8 tiles, 32 bytes each.                     */
/* Each pixel row is 4 bytes: bitplane 0, 1, 2, 3 (bit 7 = leftmost pixel). */
/* ------------------------------------------------------------------------ */
#define TILE_SKY    0
#define TILE_GRASS  1
#define TILE_BRICK  2
#define TILE_DIRT   3

static const unsigned char tiles[4 * 32] = {
    /* tile 0: sky - every pixel is colour 0 */
    0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,

    /* tile 1: grass top - 2 rows colour 2 (green), 6 rows colour 1 (brown) */
    0x00,0xFF,0x00,0x00,  0x00,0xFF,0x00,0x00,
    0xFF,0x00,0x00,0x00,  0xFF,0x00,0x00,0x00,  0xFF,0x00,0x00,0x00,
    0xFF,0x00,0x00,0x00,  0xFF,0x00,0x00,0x00,  0xFF,0x00,0x00,0x00,

    /* tile 2: bricks - colour 3 (red), colour 4 (grey mortar) */
    0x00,0x00,0xFF,0x00,                      /* row 0: mortar       */
    0x7F,0x7F,0x80,0x00,                      /* row 1: mortar at x0 */
    0x7F,0x7F,0x80,0x00,                      /* row 2               */
    0x7F,0x7F,0x80,0x00,                      /* row 3               */
    0x00,0x00,0xFF,0x00,                      /* row 4: mortar       */
    0xF7,0xF7,0x08,0x00,                      /* row 5: mortar at x4 */
    0xF7,0xF7,0x08,0x00,                      /* row 6               */
    0xF7,0xF7,0x08,0x00,                      /* row 7               */

    /* tile 3: dirt - all colour 1 (brown) */
    0xFF,0x00,0x00,0x00,  0xFF,0x00,0x00,0x00,  0xFF,0x00,0x00,0x00,  0xFF,0x00,0x00,0x00,
    0xFF,0x00,0x00,0x00,  0xFF,0x00,0x00,0x00,  0xFF,0x00,0x00,0x00,  0xFF,0x00,0x00,0x00
};

/* ------------------------------------------------------------------------ */
/* Palette (16 entries, SMS CRAM format, 00BBGGRR). Used for BG and sprites. */
/* ------------------------------------------------------------------------ */
static const unsigned char palette[16] = {
    RGB(1,2,3),   /* 0: sky (also backdrop, shows in the blanked left column) */
    RGB(2,1,0),   /* 1: dirt brown  */
    RGB(0,3,0),   /* 2: grass green */
    RGB(3,0,0),   /* 3: brick red   */
    RGB(2,2,2),   /* 4: mortar grey */
    0,0,0,0,0,0,0,0,0,0,0
};

/* ------------------------------------------------------------------------ */
/* Level data: 64 columns. Ground and brick platforms are described per     */
/* column, so no large 2D table is needed.                                  */
/* ------------------------------------------------------------------------ */

/* first ground row for each column (20 = normal ground, 0 = pit / no ground) */
static const unsigned char level_ground_row[64] = {
    20,20,20,20,20,20,20,20,     /* cols  0- 7 */
    20,20,20,20,20,20,20,20,     /* cols  8-15 */
    20,20,20,20, 0, 0, 0,20,     /* cols 16-23: pit at 20-22 */
    20,20,20,20,20,20,20,20,     /* cols 24-31 */
    20,20,20,20,20,20,20,20,     /* cols 32-39 */
    20,20,20,20, 0, 0, 0,20,     /* cols 40-47: pit at 44-46 */
    20,20,20,20,20,20,20,20,     /* cols 48-55 */
    20,20,20,20,20,20,20,20      /* cols 56-63 */
};

/* row of a one-tile-high brick platform in each column (0 = none) */
static const unsigned char level_brick_row[64] = {
     0, 0, 0, 0, 0, 0, 0, 0,     /* cols  0- 7 */
    16,16,16,16, 0, 0, 0, 0,     /* cols  8-15 */
     0, 0, 0, 0, 0, 0, 0, 0,     /* cols 16-23 */
     0, 0,13,13,13,13,13,13,     /* cols 24-31 */
     0, 0, 0, 0,16,16,16,16,     /* cols 32-39 */
     0, 0, 0, 0, 0, 0, 0, 0,     /* cols 40-47 */
     0, 0,12,12,12,12, 0, 0,     /* cols 48-55 */
     0, 0, 0, 0, 0, 0, 0, 0      /* cols 56-63 */
};

/* returns the tile index for a level cell */
static unsigned char level_tile(unsigned char col, unsigned char row)
{
    unsigned char g = level_ground_row[col];

    if (level_brick_row[col] != 0 && level_brick_row[col] == row)
        return TILE_BRICK;
    if (g != 0 && row >= g)
        return (row == g) ? TILE_GRASS : TILE_DIRT;
    return TILE_SKY;
}

/*
 * Writes level column 'col' into tilemap column 'slot' (0..31), rows 0..23.
 * All VRAM writes happen with interrupts disabled so the VDP address latch
 * cannot be disturbed by the frame interrupt handler. Callers must invoke
 * this only during VBlank (or while the display is off).
 */
static void draw_column(unsigned char slot, unsigned char col)
{
    unsigned char row;

    __asm
        di
    __endasm;

    for (row = 0; row < 24; row++) {
        SMS_setNextTileatXY(slot, row);
        SMS_setTile(level_tile(col, row));
    }

    __asm
        ei
    __endasm;
}

/* ------------------------------------------------------------------------ */
/* Main                                                                     */
/* ------------------------------------------------------------------------ */
void main(void)
{
    int camX;
    unsigned char s;

    SMS_displayOff();

    /* Blank the leftmost 8 pixels. This hides the tilemap column whose
       contents are still changing (the wrap seam of a 32-column map). */
    SMS_VDPturnOnFeature(VDPFEATURE_LEFTCOLBLANK);

    SMS_loadTiles(tiles, 0, sizeof(tiles));
    SMS_loadBGPalette(palette);
    SMS_loadSpritePalette(palette);
    SMS_setBackdropColor(0);

    /* Fill the whole 32-column tilemap with level columns 0..31 */
    for (s = 0; s < 32; s++)
        draw_column(s, s);

    /* camX starts at -8: the first 8 px of the screen are blanked and show
       nothing from the level, so level column 0 appears right after that. */
    camX = -8;
    SMS_setBGScrollX((unsigned char)(-camX));

    SMS_displayOn();

    for (;;) {
        SMS_waitForVBlank();

        /* On every 8th pixel, write the next level column into the tilemap
           slot that just left the screen. The screen mapping is:
           screen x shows level pixel (camX + x). Slot a (a = camX/8) is
           only visible in its blanked part when camX = 8a, so writing
           level column a+32 there is invisible until it scrolls in. */
        if (camX >= 0 && camX < 256 && (camX & 7) == 0) {
            unsigned char a = (unsigned char)(camX >> 3);   /* 0..31 */
            draw_column(a, a + 32);                         /* level col 32..63 */
        }

        SMS_setBGScrollX((unsigned char)(-camX));

        /* Scroll 1 px per frame until the right edge of the level is reached */
        if (camX < 256)
            camX++;
    }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
