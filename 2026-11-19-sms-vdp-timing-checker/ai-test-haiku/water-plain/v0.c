/* ============================================================================
 * Animated water + bobbing boat for Sega Master System
 * devkitSMS (SMSlib) / SDCC
 *
 *   sdcc -c -mz80 main.c
 *   (link with crt0_sms.rel and SMSlib.lib, then makesms)
 *
 * - Background: whole 32x28 tilemap filled with tile 0 (water).
 * - Water tile 0 is rewritten in VRAM with the next of 4 animation frames
 *   every 8 video frames.
 * - Boat: 16x16 metasprite built from four 8x8 hardware sprites, bobbing
 *   up and down in the middle of the screen.
 * - All tiles and palettes are C arrays; no external assets.
 * ========================================================================== */

#include <SMSlib.h>

#define BOAT_X  120     /* left edge of the 16x16 boat */
#define BOAT_Y0 84      /* base Y of the boat (bobbing is added to it) */

/* ---------------------------------------------------------------------------
 * Palettes (SMS CRAM: 6-bit colour, RGB(r,g,b) with each component 0..3)
 * ------------------------------------------------------------------------- */

/* background palette: entry 0 is also the backdrop colour */
const unsigned char bg_palette[16] = {
    RGB(0,0,1),     /* 0: backdrop / unused */
    RGB(0,0,2),     /* 1: deep water */
    RGB(0,1,3),     /* 2: water */
    RGB(1,2,3),     /* 3: light water */
    RGB(3,3,3)      /* 4: foam */
    /* 5..15 are zero */
};

/* sprite palette: entry 0 is transparent for sprites */
const unsigned char spr_palette[16] = {
    0,              /* 0: transparent */
    RGB(0,0,0),     /* 1: outline / mast */
    RGB(2,1,0),     /* 2: wood brown */
    RGB(3,3,3),     /* 3: white sail / cabin */
    RGB(3,0,0)      /* 4: red accent */
    /* 5..15 are zero */
};

/* ---------------------------------------------------------------------------
 * Water animation frames: 4 frames of one 8x8 tile, colour indices 0..15.
 * Each frame is a diagonal wave that shifts by 2 pixels per frame.
 * The tile is seamless (wraps modulo 8 in both directions).
 * ------------------------------------------------------------------------- */
const unsigned char water_frames[4][8][8] = {
    { /* frame 0 */
        {4,3,2,1,1,1,2,3},
        {3,2,1,1,1,2,3,4},
        {2,1,1,1,2,3,4,3},
        {1,1,1,2,3,4,3,2},
        {1,1,2,3,4,3,2,1},
        {1,2,3,4,3,2,1,1},
        {2,3,4,3,2,1,1,1},
        {3,4,3,2,1,1,1,2}
    },
    { /* frame 1 */
        {2,1,1,1,2,3,4,3},
        {1,1,1,2,3,4,3,2},
        {1,1,2,3,4,3,2,1},
        {1,2,3,4,3,2,1,1},
        {2,3,4,3,2,1,1,1},
        {3,4,3,2,1,1,1,2},
        {4,3,2,1,1,1,2,3},
        {3,2,1,1,1,2,3,4}
    },
    { /* frame 2 */
        {1,1,2,3,4,3,2,1},
        {1,2,3,4,3,2,1,1},
        {2,3,4,3,2,1,1,1},
        {3,4,3,2,1,1,1,2},
        {4,3,2,1,1,1,2,3},
        {3,2,1,1,1,2,3,4},
        {2,1,1,1,2,3,4,3},
        {1,1,1,2,3,4,3,2}
    },
    { /* frame 3 */
        {2,3,4,3,2,1,1,1},
        {3,4,3,2,1,1,1,2},
        {4,3,2,1,1,1,2,3},
        {3,2,1,1,1,2,3,4},
        {2,1,1,1,2,3,4,3},
        {1,1,1,2,3,4,3,2},
        {1,1,2,3,4,3,2,1},
        {1,2,3,4,3,2,1,1}
    }
};

/* ---------------------------------------------------------------------------
 * Boat: 16x16 colour-index art (0 = transparent).
 * Split into four 8x8 quadrants:
 *   tile 1 = top-left, tile 2 = top-right,
 *   tile 3 = bottom-left, tile 4 = bottom-right
 * ------------------------------------------------------------------------- */
const unsigned char boat_pixels[16][16] = {
    {0,0,0,0,0,0,0,1, 0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,1, 4,4,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,1, 3,4,4,0,0,0,0,0},
    {0,0,0,0,0,0,0,1, 3,3,4,4,0,0,0,0},
    {0,0,0,0,0,0,0,1, 3,3,3,3,4,0,0,0},
    {0,0,0,0,0,0,0,1, 3,3,3,3,3,3,0,0},
    {0,0,0,0,0,0,0,1, 3,3,3,3,3,3,3,0},
    {0,0,0,0,0,0,0,1, 3,3,3,3,3,3,3,3},
    {0,0,0,0,0,0,0,1, 1,1,1,1,1,1,1,1},
    {0,0,1,1,1,1,1,1, 1,1,1,1,1,1,0,0},
    {0,1,2,2,2,2,2,2, 2,2,2,2,2,2,1,0},
    {0,0,1,2,2,2,2,2, 2,2,2,2,2,1,0,0},
    {0,0,0,1,1,1,1,1, 1,1,1,1,1,0,0,0},
    {0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0}
};

/* Vertical bobbing offsets (0..6), 16 entries, looped */
static const unsigned char bob_table[16] = {
    4,5,6,6,6,5,4,3,2,1,0,0,0,1,2,3
};

/* ---------------------------------------------------------------------------
 * RAM buffers holding the 4bpp-packed tile data ready for VRAM
 * ------------------------------------------------------------------------- */
static unsigned char water_packed[4][32];
static unsigned char boat_packed[4][32];

/* ---------------------------------------------------------------------------
 * Convert an 8x8 block of colour indices (0..15) into SMS 4bpp planar tile
 * format: 8 rows x 4 bytes, one byte per bitplane per row, MSB = leftmost px.
 *   px     : pointer to top-left pixel of the 8x8 block
 *   stride : distance in bytes between rows in the source array
 *   out    : 32 bytes destination
 * ------------------------------------------------------------------------- */
static void pack_tile(const unsigned char *px, unsigned char stride, unsigned char *out)
{
    unsigned char row, col, v, bit;
    unsigned char p0, p1, p2, p3;

    for (row = 0; row < 8; row++) {
        p0 = p1 = p2 = p3 = 0;
        for (col = 0; col < 8; col++) {
            v   = px[(unsigned int)row * stride + col];
            bit = (unsigned char)(0x80 >> col);
            if (v & 1) p0 |= bit;
            if (v & 2) p1 |= bit;
            if (v & 4) p2 |= bit;
            if (v & 8) p3 |= bit;
        }
        out[row * 4 + 0] = p0;
        out[row * 4 + 1] = p1;
        out[row * 4 + 2] = p2;
        out[row * 4 + 3] = p3;
    }
}

/* ---------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
void main(void)
{
    unsigned char frame;
    unsigned char water_frame;
    unsigned char shown_frame;
    unsigned char bob;
    unsigned char boat_y;
    unsigned char t;

    /* 1. Pack all tile graphics into 4bpp format */
    for (t = 0; t < 4; t++)
        pack_tile(&water_frames[t][0][0], 8, water_packed[t]);

    for (t = 0; t < 4; t++)
        pack_tile(&boat_pixels[(t >> 1) * 8][(t & 1) * 8], 16, boat_packed[t]);

    /* 2. Sprites use the first 256 tiles, same as the background */
    SMS_useFirstHalfTilesforSprites(1);

    /* 3. Palettes */
    SMS_loadBGPalette(bg_palette);
    SMS_loadSpritePalette(spr_palette);

    /* 4. Tiles: water in tile 0, boat in tiles 1..4 */
    SMS_loadTiles(water_packed[0], 0, 32);
    for (t = 0; t < 4; t++)
        SMS_loadTiles(boat_packed[t], 1 + t, 32);

    /* 5. Fill the whole background tilemap with tile 0 (water).
     *    Entry = 0x0000 means tile 0, no flips, BG palette. */
    SMS_VRAMmemset(SMS_PNTAddress, 0, 0x700);   /* 32x28 entries * 2 bytes */

    /* 6. Sprites and display */
    SMS_initSprites();
    SMS_displayOn();

    frame = 0;
    shown_frame = 0;

    /* 7. Main loop: once per video frame */
    while (1) {
        SMS_waitForVBlank();

        /* Water: advance to the next frame every 8 video frames.
         * (frame >> 3) & 3 cycles 0,1,2,3 every 32 frames.
         * Only touch VRAM when the frame actually changes. */
        water_frame = (frame >> 3) & 3;
        if (water_frame != shown_frame) {
            SMS_loadTiles(water_packed[water_frame], 0, 32);
            shown_frame = water_frame;
        }

        /* Boat: bob changes every 4 video frames */
        bob = bob_table[(frame >> 2) & 15];
        boat_y = BOAT_Y0 + bob;

        /* Build the 16x16 metasprite from four 8x8 sprites */
        SMS_initSprites();
        SMS_addSprite(BOAT_X,     boat_y,     1);
        SMS_addSprite(BOAT_X + 8, boat_y,     2);
        SMS_addSprite(BOAT_X,     boat_y + 8, 3);
        SMS_addSprite(BOAT_X + 8, boat_y + 8, 4);
        SMS_copySpritestoSAT();

        frame++;
    }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
