/* main.c - Sega Master System shoot-em-up demo (devkitSMS / SMSlib, SDCC)
 *
 *  - 40 bullets (8x8 hardware sprites) spiral out from the screen centre and
 *    wrap around the screen edges.
 *  - 64 stars are drawn as background tiles. They drop one tile row every
 *    8 frames. The stars are split in four groups of 16, moved on frames
 *    0..3 of the 8-frame cycle, so the VRAM writes always fit safely inside
 *    VBlank on real hardware.
 *  - All VRAM access happens either with the display off (init) or right
 *    after SMS_waitForVBlank().
 *  - Sprite draw order is rotated every frame so the 8-sprites-per-scanline
 *    limit makes bullets flicker evenly instead of dropping the same ones.
 *  - Bullet movement uses a precomputed sine*3 table so the per-frame logic
 *    (and the start-up warm-up) is fast enough to finish within a frame.
 *
 * Build:
 *   sdcc -c -mz80 main.c
 *   (link with crt0_sms.rel and SMSlib.lib, then makesms)
 */

#include "SMSlib.h"

#define NUM_BULLETS   40
#define NUM_STARS     64
#define STAR_GROUP    16
#define MAP_ROWS      24   /* visible tile rows */

/* ------------------------------------------------------------------ */
/* Tile data (4bpp planar: 4 bytes per row, 8 rows per tile)           */
/* ------------------------------------------------------------------ */

/* Background tiles: 0 = blank, 1 = dim star, 2 = bright star */
static const unsigned char bg_tiles[96] = {
    /* tile 0: blank */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    /* tile 1: dim star (single pixel, colour 1) */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0x10,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    /* tile 2: bright star (plus shape, colour 1 arms, colour 3 centre) */
    0,0,0,0, 0,0,0,0, 0x10,0,0,0, 0x38,0x10,0,0,
    0x10,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0
};

/* Sprite tile: round bullet (outer colour 1, inner colour 3) */
static const unsigned char bullet_tile[32] = {
    0x3C,0x00,0,0,
    0x7E,0x3C,0,0,
    0xFF,0x7E,0,0,
    0xFF,0x7E,0,0,
    0xFF,0x7E,0,0,
    0xFF,0x7E,0,0,
    0x7E,0x3C,0,0,
    0x3C,0x00,0,0
};

/* ------------------------------------------------------------------ */
/* Palettes (16 entries each)                                          */
/* ------------------------------------------------------------------ */
static const unsigned char bg_palette[16] = {
    RGB(0,0,0), RGB(1,1,2), RGB(2,2,3), RGB(3,3,3),
    0,0,0,0, 0,0,0,0, 0,0,0,0
};

static const unsigned char spr_palette[16] = {
    RGB(0,0,0), RGB(3,1,0), RGB(3,3,0), RGB(3,3,2),
    0,0,0,0, 0,0,0,0, 0,0,0,0
};

/* Quarter sine wave, 0..90 degrees in 16 steps (+1), amplitude 127 */
static const signed char sine_q[17] = {
    0, 12, 25, 37, 49, 60, 71, 81, 90, 98, 106, 112, 117, 122, 125, 126, 127
};

/* ------------------------------------------------------------------ */
/* State                                                               */
/* ------------------------------------------------------------------ */
static unsigned int  bx[NUM_BULLETS];   /* 8.8 fixed point, wraps at 256 px   */
static unsigned int  by[NUM_BULLETS];   /* 8.8 fixed point, 0..192*256-1      */
static unsigned char bang[NUM_BULLETS]; /* angle, 256 steps per revolution    */

static unsigned char star_y[NUM_STARS]; /* tile row; column is (i & 31)       */
static unsigned char star_t[NUM_STARS]; /* tile index 1 or 2                  */

static int           sin3[64];          /* sin64(a) * 3, built at start-up    */

static unsigned int  seed;
static unsigned char frame_count;
static unsigned char rot;

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */
static unsigned char rnd(void)
{
    seed = seed * 25173u + 13849u;
    return (unsigned char)(seed >> 8);
}

/* sine with 64 steps per revolution (only used to build the table) */
static int sin64(unsigned char a)
{
    unsigned char i;
    a &= 63;
    i = a & 15;
    switch (a >> 4) {
        case 0:  return  sine_q[i];
        case 1:  return  sine_q[16 - i];
        case 2:  return -sine_q[i];
        default: return -sine_q[16 - i];
    }
}

static void build_sin_table(void)
{
    unsigned char a;
    for (a = 0; a < 64; a++)
        sin3[a] = sin64(a) * 3;
}

static void bullet_step(unsigned char i, unsigned char turn)
{
    unsigned char a = bang[i] >> 2;
    int dx = sin3[(unsigned char)(a + 16) & 63];   /* cosine */
    int dy = sin3[a];
    unsigned int y;

    bx[i] += (unsigned int)dx;    /* 256 px wide: natural 16-bit wrap */

    y = by[i] + (unsigned int)dy;
    if (y >= 49152u) {            /* left the 0..191 range */
        if (dy < 0)
            y -= 16384u;          /* wrapped below 0 -> bottom */
        else
            y -= 49152u;          /* ran past bottom -> top */
    }
    by[i] = y;

    if (turn)
        bang[i]++;
}

static unsigned int star_addr(unsigned char col, unsigned char row)
{
    return SMS_PNTAddress | ((((unsigned int)row << 5) + col) << 1);
}

static void build_sprites(void)
{
    unsigned char j, idx;
    SMS_initSprites();
    idx = rot;
    for (j = 0; j < NUM_BULLETS; j++) {
        SMS_addSprite((unsigned char)(bx[idx] >> 8),
                      (unsigned char)(by[idx] >> 8), 0);
        idx++;
        if (idx >= NUM_BULLETS)
            idx = 0;
    }
    rot++;
    if (rot >= NUM_BULLETS)
        rot = 0;
}

/* Move 16 stars (starting at 'first') one row down.
   Must be called during VBlank. */
static void move_star_group(unsigned char first)
{
    unsigned char i, col, row, last;
    last = first + STAR_GROUP;
    for (i = first; i < last; i++) {
        col = i & 31;
        row = star_y[i];
        SMS_setAddr(star_addr(col, row));
        SMS_setTile(0);
        row++;
        if (row >= MAP_ROWS)
            row = 0;
        star_y[i] = row;
        SMS_setAddr(star_addr(col, row));
        SMS_setTile(star_t[i]);
    }
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */
void main(void)
{
    unsigned char i;
    unsigned int n;
    unsigned char k;

    SMS_displayOff();
    SMS_setSpriteMode(SPRITEMODE_NORMAL);
    SMS_useFirstHalfTilesforSprites(0);   /* sprite tile 0 -> VRAM tile 256 */
    SMS_setBGScrollX(0);
    SMS_setBGScrollY(0);

    SMS_loadBGPalette(bg_palette);
    SMS_loadSpritePalette(spr_palette);
    SMS_setBackdropColor(0);

    SMS_loadTiles(bg_tiles, 0, sizeof(bg_tiles));
    SMS_loadTiles(bullet_tile, 256, sizeof(bullet_tile));

    build_sin_table();

    /* clear the whole name table (32x28 entries) - display is off */
    SMS_setAddr(XYtoADDR(0, 0));
    for (n = 0; n < 32u * 28u; n++)
        SMS_setTile(0);

    /* stars: two per column, 12 rows apart, so they can never overlap */
    seed = 0x1D2B;
    for (i = 0; i < 32; i++) {
        star_y[i]      = rnd() % MAP_ROWS;
        star_y[i + 32] = (star_y[i] + 12) % MAP_ROWS;
        star_t[i]      = 1 + (rnd() & 1);
        star_t[i + 32] = 1 + (rnd() & 1);
    }
    for (i = 0; i < NUM_STARS; i++) {
        SMS_setAddr(star_addr(i & 31, star_y[i]));
        SMS_setTile(star_t[i]);
    }

    /* bullets: start at the centre, staggered along a spiral */
    for (i = 0; i < NUM_BULLETS; i++) {
        bx[i]   = (unsigned int)124 << 8;
        by[i]   = (unsigned int)92 << 8;
        bang[i] = i * 6;
        for (k = 0; k < (unsigned char)(i * 3); k++)
            bullet_step(i, (k & 3) == 0);
    }

    frame_count = 0;
    rot = 0;
    build_sprites();

    SMS_VDPturnOnFeature(VDPFEATURE_FRAMEIRQ | VDPFEATURE_SHOWDISPLAY);

    for (;;) {
        SMS_waitForVBlank();

        /* --- VBlank: all VRAM writes happen here --- */
        SMS_copySpritestoSAT();
        k = frame_count & 7;
        if (k < 4)
            move_star_group(k * STAR_GROUP);

        /* --- active display: game logic only, no VDP access --- */
        frame_count++;
        for (i = 0; i < NUM_BULLETS; i++)
            bullet_step(i, (frame_count & 3) == 0);
        build_sprites();
    }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
