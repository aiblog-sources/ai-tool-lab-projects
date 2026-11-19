/* main.c - animated water background and bobbing boat for the Sega Master System
 *
 * Build:  sdcc -c -mz80 main.c
 *         link with crt0_sms.rel and SMSlib.lib, then makesms
 *
 * Background: the whole 32x24 tilemap uses tile 0. Every 8 frames the graphics
 *             of tile 0 are replaced with the next of 4 water frames.
 * Sprite:     a 16x16 boat made of four 8x8 sprites bobs up and down.
 *
 * All tile graphics are defined as 8x8 / 16x16 colour-index arrays and packed
 * into SMS 4bpp planar format at startup. All VRAM writes happen right after
 * SMS_waitForVBlank(), so they are safe on real hardware.
 */

#include <SMSlib.h>

/* ------------------------------------------------------------------------- */
/* Palettes (SMS colour format: --BBGGRR, 0..3 per channel)                   */
/* ------------------------------------------------------------------------- */

const unsigned char bg_palette[16] = {
    RGB(0,0,1),   /* 0: backdrop / deepest water        */
    RGB(0,1,2),   /* 1: dark water                      */
    RGB(1,2,3),   /* 2: mid water                       */
    RGB(2,3,3),   /* 3: foam highlight                  */
    RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0),
    RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0),
    RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0)
};

const unsigned char spr_palette[16] = {
    RGB(0,0,0),   /* 0: transparent                     */
    RGB(2,1,0),   /* 1: brown hull                      */
    RGB(3,3,3),   /* 2: white sail                      */
    RGB(3,0,0),   /* 3: red flag                        */
    RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0),
    RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0),
    RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0)
};

/* ------------------------------------------------------------------------- */
/* Water animation: 4 frames of one 8x8 tile (colour indices 0..3)            */
/* ------------------------------------------------------------------------- */

const unsigned char water_px[4][8][8] = {
    { /* frame 0 */
        {1,1,2,2,3,2,2,1},
        {1,2,2,3,2,2,1,1},
        {2,2,3,2,2,1,1,1},
        {2,3,2,2,1,1,1,2},
        {3,2,2,1,1,1,2,2},
        {2,2,1,1,1,2,2,3},
        {2,1,1,1,2,2,3,2},
        {1,1,1,2,2,3,2,2}
    },
    { /* frame 1 */
        {2,1,1,1,2,2,3,2},
        {1,1,1,2,2,3,2,2},
        {1,1,2,2,3,2,2,1},
        {1,2,2,3,2,2,1,1},
        {2,2,3,2,2,1,1,1},
        {2,3,2,2,1,1,1,2},
        {3,2,2,1,1,1,2,2},
        {2,2,1,1,1,2,2,3}
    },
    { /* frame 2 */
        {3,2,2,1,1,1,2,2},
        {2,2,1,1,1,2,2,3},
        {2,1,1,1,2,2,3,2},
        {1,1,1,2,2,3,2,2},
        {1,1,2,2,3,2,2,1},
        {1,2,2,3,2,2,1,1},
        {2,2,3,2,2,1,1,1},
        {2,3,2,2,1,1,1,2}
    },
    { /* frame 3 */
        {2,2,3,2,2,1,1,1},
        {2,3,2,2,1,1,1,2},
        {3,2,2,1,1,1,2,2},
        {2,2,1,1,1,2,2,3},
        {2,1,1,1,2,2,3,2},
        {1,1,1,2,2,3,2,2},
        {1,1,2,2,3,2,2,1},
        {1,2,2,3,2,2,1,1}
    }
};

/* ------------------------------------------------------------------------- */
/* Boat: 16x16 colour-index image (0 = transparent)                           */
/* ------------------------------------------------------------------------- */

const unsigned char boat_px[16][16] = {
    {0,0,0,0,0,0,0,1,3,3,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,1,3,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,1,2,2,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,1,2,2,2,0,0,0,0,0},
    {0,0,0,0,0,0,0,1,2,2,2,2,0,0,0,0},
    {0,0,0,0,0,0,0,1,2,2,2,2,2,0,0,0},
    {0,0,0,0,0,0,0,1,2,2,2,2,2,2,0,0},
    {0,0,0,0,0,0,0,1,2,2,2,0,0,0,0,0},
    {0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0},
    {0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0},
    {0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
    {0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0},
    {0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
};

/* Bobbing offsets (pixels), 16-step cycle */
const signed char bob_table[16] = {
    0, 1, 2, 3, 4, 3, 2, 1, 0, -1, -2, -3, -4, -3, -2, -1
};

/* ------------------------------------------------------------------------- */
/* Settings                                                                   */
/* ------------------------------------------------------------------------- */

#define WATER_FRAME_PERIOD   8    /* VBlanks per water frame                 */
#define BOB_PERIOD           4    /* VBlanks per bob step                    */
#define BOAT_X               120  /* screen X of boat top-left               */
#define BOAT_Y               84   /* screen Y of boat top-left (before bob)  */
#define SPRITE_TILE_BASE     256  /* sprite tiles start at VRAM tile 256     */

/* RAM buffers holding packed 4bpp data (32 bytes per 8x8 tile) */
static unsigned char water_tiles[4][32];
static unsigned char boat_tiles[4 * 32];

/* Pack an 8x8 block of colour indices into SMS 4bpp planar format.
   Each row is 4 bytes: plane0, plane1, plane2, plane3. MSB is leftmost pixel. */
static void pack_8x8(const unsigned char *src, unsigned char stride, unsigned char *dst)
{
    unsigned char r, c, p, bits;

    for (r = 0; r < 8; r++) {
        for (p = 0; p < 4; p++) {
            bits = 0;
            for (c = 0; c < 8; c++) {
                if ((src[r * stride + c] >> p) & 1)
                    bits |= (unsigned char)(0x80 >> c);
            }
            dst[r * 4 + p] = bits;
        }
    }
}

void main(void)
{
    unsigned char f, k;
    unsigned char water_frame = 0;
    unsigned char water_timer = 0;
    unsigned char bob_idx = 0;
    unsigned char bob_timer = 0;
    int by;

    SMS_displayOff();

    /* Sprites use the second half of VRAM tiles (256..511) */
    SMS_useFirstHalfTilesforSprites(0);

    /* Palettes */
    SMS_loadBGPalette(bg_palette);
    SMS_loadSpritePalette(spr_palette);

    /* Pack water frames and load frame 0 into tile 0 */
    for (f = 0; f < 4; f++) {
        pack_8x8(&water_px[f][0][0], 8, water_tiles[f]);
    }
    SMS_loadTiles(water_tiles[0], 0, 32);

    /* Pack the four 8x8 boat quarters (TL, TR, BL, BR) and load them */
    for (k = 0; k < 4; k++) {
        const unsigned char *s = &boat_px[0][0] + ((k >> 1) * 8 * 16) + ((k & 1) * 8);
        pack_8x8(s, 16, boat_tiles + k * 32);
    }
    SMS_loadTiles(boat_tiles, SPRITE_TILE_BASE, 4 * 32);

    /* Fill the whole background tilemap (32x24 cells, 2 bytes each) with tile 0 */
    SMS_VRAMmemset(SMS_PNTAddress, 0, 32 * 24 * 2);

    SMS_initSprites();
    SMS_displayOn();

    while (1) {
        SMS_waitForVBlank();

        /* ---- All VRAM access below happens during VBlank ---- */

        /* Push last frame's sprite list to the SAT */
        SMS_copySpritestoSAT();

        /* Water: swap tile 0 graphics every WATER_FRAME_PERIOD frames */
        if (++water_timer >= WATER_FRAME_PERIOD) {
            water_timer = 0;
            water_frame = (water_frame + 1) & 3;
            SMS_loadTiles(water_tiles[water_frame], 0, 32);
        }

        /* Boat bobbing */
        if (++bob_timer >= BOB_PERIOD) {
            bob_timer = 0;
            bob_idx = (bob_idx + 1) & 15;
        }

        /* Build the sprite list for the next frame */
        SMS_initSprites();
        by = BOAT_Y + bob_table[bob_idx];
        SMS_addSprite(BOAT_X,     by,     SPRITE_TILE_BASE - SPRITE_TILE_BASE + 0);
        SMS_addSprite(BOAT_X + 8, by,     1);
        SMS_addSprite(BOAT_X,     by + 8, 2);
        SMS_addSprite(BOAT_X + 8, by + 8, 3);
    }
}

/* ------------------------------------------------------------------------- */
/* SEGA header (product code 9999, revision 0)                                */
/* ------------------------------------------------------------------------- */
SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
