/* ============================================================================
 * Sega Master System: score HUD + 8 bouncing enemy sprites
 *
 * Build:
 *   sdcc -c -mz80 main.c
 *   (link with devkitSMS crt0_sms.rel and SMSlib.lib, then run makesms)
 *
 * Real-hardware notes:
 *   - All VRAM writes happen only in the VBlank window (after
 *     SMS_waitForVBlank), so there is no tearing or garbage on screen.
 *   - Each enemy sits on its own scanline, so no line ever has more than
 *     one sprite and the 8-sprites-per-line limit cannot cause flicker.
 *   - Tiles and palettes are plain const arrays, so there are no external
 *     assets and no emulator-only features are used.
 * ============================================================================ */

#include "SMSlib.h"

/* SMSlib only defines the VDP control port (0xBF). Define the data port (0xBE). */
__sfr __at (0xBE) VDPDataPort;

/* ---------------------------------------------------------------------------
 * Configuration
 * ------------------------------------------------------------------------- */
#define HUD_ROW             0     /* top row of the screen */
#define HUD_LABEL_X         2     /* "SCORE" starts at column 2 */
#define HUD_DIGITS_X        8     /* six digits start at column 8 */
#define SCORE_DIGITS        6

#define DIGIT_TILE0         1     /* tiles 1..10 = '0'..'9' */
#define TILE_LETTER_S       11
#define TILE_LETTER_C       12
#define TILE_LETTER_O       13
#define TILE_LETTER_R       14
#define TILE_LETTER_E       15

#define ENEMY_COUNT         8
#define ENEMY_VRAM_TILE     256   /* sprite tile 0 when sprites use the second half */
#define ENEMY_MIN_X         0
#define ENEMY_MAX_X         248   /* 248 + 8 pixels = right screen edge */

/* ---------------------------------------------------------------------------
 * Background font: 1bpp glyphs, 8 bytes each, bit 7 = leftmost pixel.
 * Index = tile number. Tile 0 is blank.
 * ------------------------------------------------------------------------- */
static const unsigned char font_1bpp[16][8] = {
    { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 }, /*  0: blank */
    { 0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00 }, /*  1: '0' */
    { 0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00 }, /*  2: '1' */
    { 0x3C,0x66,0x06,0x0C,0x18,0x30,0x7E,0x00 }, /*  3: '2' */
    { 0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00 }, /*  4: '3' */
    { 0x06,0x0E,0x1E,0x66,0x7F,0x06,0x06,0x00 }, /*  5: '4' */
    { 0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00 }, /*  6: '5' */
    { 0x3C,0x66,0x60,0x7C,0x66,0x66,0x3C,0x00 }, /*  7: '6' */
    { 0x7E,0x66,0x0C,0x18,0x18,0x18,0x18,0x00 }, /*  8: '7' */
    { 0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00 }, /*  9: '8' */
    { 0x3C,0x66,0x66,0x3E,0x06,0x66,0x3C,0x00 }, /* 10: '9' */
    { 0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00 }, /* 11: 'S' */
    { 0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00 }, /* 12: 'C' */
    { 0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00 }, /* 13: 'O' */
    { 0x7C,0x66,0x66,0x7C,0x78,0x6C,0x66,0x00 }, /* 14: 'R' */
    { 0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00 }, /* 15: 'E' */
};

/* ---------------------------------------------------------------------------
 * Enemy sprite: 8x8, already in SMS 4bpp planar format (32 bytes).
 * Each row is 4 bytes: plane0, plane1, plane2, plane3.
 * Pixels that use color index 1 set plane0 only.
 * ------------------------------------------------------------------------- */
static const unsigned char enemy_tile_4bpp[32] = {
    0x3C,0,0,0,
    0x7E,0,0,0,
    0xDB,0,0,0,
    0xFF,0,0,0,
    0x24,0,0,0,
    0x5A,0,0,0,
    0x81,0,0,0,
    0x42,0,0,0
};

/* ---------------------------------------------------------------------------
 * Palettes: SMS CRAM format is --BBGGRR (2 bits per channel).
 * ------------------------------------------------------------------------- */
/* Background: entry 0 = black (transparent/backdrop), entry 1 = white */
static const unsigned char bg_palette[16] = {
    0x00, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* Sprites: entry 0 = transparent, entry 1 = red */
static const unsigned char spr_palette[16] = {
    0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* ---------------------------------------------------------------------------
 * Enemy layout: each enemy has its own scanline.
 * ------------------------------------------------------------------------- */
static const unsigned char enemy_y[ENEMY_COUNT] = {
    32, 48, 64, 80, 96, 112, 128, 144
};

/* ---------------------------------------------------------------------------
 * Game state (RAM)
 * ------------------------------------------------------------------------- */
static unsigned char score_digits[SCORE_DIGITS];   /* [0] = 100000s ... [5] = units */

static int enemy_x[ENEMY_COUNT]  = { 0, 32, 64, 96, 128, 160, 192, 224 };
static int enemy_dx[ENEMY_COUNT] = { 1, -1, 2, -2, 1, -1, 2, -2 };

/* ---------------------------------------------------------------------------
 * Load one 1bpp glyph into background tile 'tile' as 4bpp.
 * Color index 1 -> plane0 only.
 * ------------------------------------------------------------------------- */
static void load_glyph(unsigned char tile, const unsigned char *g)
{
    unsigned char r;

    SMS_setAddr(TILEtoADDR(tile));
    for (r = 0; r < 8; r++) {
        VDPDataPort = g[r];   /* plane 0 */
        VDPDataPort = 0;      /* plane 1 */
        VDPDataPort = 0;      /* plane 2 */
        VDPDataPort = 0;      /* plane 3 */
    }
}

/* ---------------------------------------------------------------------------
 * Score: add 10 (increments the tens column with carry).
 * Wraps from 999999 back to 000000.
 * ------------------------------------------------------------------------- */
static void score_add_10(void)
{
    unsigned char i = SCORE_DIGITS - 2;   /* tens column */

    for (;;) {
        score_digits[i]++;
        if (score_digits[i] <= 9) {
            return;
        }
        score_digits[i] = 0;
        if (i == 0) {
            return;                       /* wrap at 1000000 */
        }
        i--;
    }
}

/* ---------------------------------------------------------------------------
 * Game logic. Runs outside VBlank and does NO VRAM access.
 * ------------------------------------------------------------------------- */
static void game_update(void)
{
    unsigned char i;

    score_add_10();

    for (i = 0; i < ENEMY_COUNT; i++) {
        enemy_x[i] += enemy_dx[i];
        if (enemy_x[i] <= ENEMY_MIN_X) {
            enemy_x[i] = ENEMY_MIN_X;
            enemy_dx[i] = -enemy_dx[i];
        } else if (enemy_x[i] >= ENEMY_MAX_X) {
            enemy_x[i] = ENEMY_MAX_X;
            enemy_dx[i] = -enemy_dx[i];
        }
    }

    /* Build the sprite list in RAM. It is copied to VRAM during VBlank. */
    SMS_initSprites();
    for (i = 0; i < ENEMY_COUNT; i++) {
        SMS_addSprite(enemy_x[i], enemy_y[i], 0);
    }
}

/* ---------------------------------------------------------------------------
 * Draw the six score digits. Call ONLY inside VBlank.
 * ------------------------------------------------------------------------- */
static void hud_draw_score(void)
{
    unsigned char i;

    SMS_setNextTileatXY(HUD_DIGITS_X, HUD_ROW);
    for (i = 0; i < SCORE_DIGITS; i++) {
        SMS_setTile(DIGIT_TILE0 + score_digits[i]);
    }
}

/* ---------------------------------------------------------------------------
 * Entry point
 * ------------------------------------------------------------------------- */
void main(void)
{
    unsigned char i;

    SMS_displayOff();
    SMS_VDPturnOnFeature(VDPFEATURE_FRAMEIRQ);   /* needed for SMS_waitForVBlank */

    /* Clear the background name table (all tiles = 0 = blank) */
    SMS_VRAMmemset(SMS_PNTAddress, 0, 1792);

    /* Load background font tiles 0..15 */
    for (i = 0; i < 16; i++) {
        load_glyph(i, font_1bpp[i]);
    }

    /* Load enemy sprite tile into VRAM tile 256 (second half) */
    SMS_loadTiles(enemy_tile_4bpp, ENEMY_VRAM_TILE, sizeof(enemy_tile_4bpp));
    SMS_useFirstHalfTilesforSprites(0);          /* sprites use tiles 256..511 */

    /* Palettes */
    SMS_loadBGPalette(bg_palette);
    SMS_loadSpritePalette(spr_palette);
    SMS_setBackdropColor(0);

    /* Static "SCORE" label on the top row */
    SMS_setNextTileatXY(HUD_LABEL_X, HUD_ROW);
    SMS_setTile(TILE_LETTER_S);
    SMS_setTile(TILE_LETTER_C);
    SMS_setTile(TILE_LETTER_O);
    SMS_setTile(TILE_LETTER_R);
    SMS_setTile(TILE_LETTER_E);

    /* Start from 000000 */
    for (i = 0; i < SCORE_DIGITS; i++) {
        score_digits[i] = 0;
    }

    SMS_displayOn();

    for (;;) {
        game_update();              /* logic + sprite list in RAM */
        SMS_waitForVBlank();        /* from here on, VRAM access is safe */
        hud_draw_score();           /* update score digits */
        SMS_copySpritestoSAT();     /* upload sprite table */
    }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
