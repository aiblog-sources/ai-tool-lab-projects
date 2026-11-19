/* ============================================================================
 * Score display + bouncing enemies for Sega Master System
 * Toolchain: devkitSMS (SMSlib) + SDCC
 *   sdcc -c -mz80 main.c
 *   (link with crt0_sms.rel and SMSlib.lib, then makesms)
 *
 * Top row  : "SCORE" label followed by a 6-digit score (background tiles).
 *            The score increases by 10 every frame.
 * Below    : 8 enemy sprites (8x8) bouncing left and right.
 *
 * All tiles and palettes are defined here as C arrays (no external assets).
 * Tiles are stored in SMS 4bpp planar format: 8 rows x 4 bytes = 32 bytes per
 * tile. Using ROW(b) below, a pixel with colour index 1 is set in plane 0 only,
 * so glyph bits map directly to palette index 1.
 * ============================================================================ */

#include "SMSlib.h"

/* One 8-pixel row in SMS 4bpp planar format: plane 0 = pattern, planes 1..3 = 0 */
#define ROW(b) (b),0,0,0

#define NUM_ENEMIES      8
#define ENEMY_TILE       0      /* sprite tile index (VRAM tile 256) */
#define ENEMY_MIN_X      0
#define ENEMY_MAX_X      248    /* 256 - 8 */
#define ENEMY_ROW1_Y     40
#define ENEMY_ROW2_Y     64

#define SCORE_LABEL_X    2
#define SCORE_DIGITS_X   8
#define SCORE_Y          0

/* ----------------------------------------------------------------------------
 * Palettes (SMS colour format: 00BBGGRR, RGB() macro from SMSlib.h)
 * ---------------------------------------------------------------------------- */

/* Background palette: index 0 = backdrop/transparent (black), 1 = white text */
static const unsigned char bg_palette[16] = {
    RGB(0,0,0),   RGB(3,3,3),   RGB(1,1,1),   RGB(2,2,2),
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0
};

/* Sprite palette: index 0 = transparent, 1 = orange enemies */
static const unsigned char spr_palette[16] = {
    RGB(0,0,0),   RGB(3,1,0),   RGB(3,2,1),   RGB(2,0,0),
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0
};

/* ----------------------------------------------------------------------------
 * Background tiles
 *   0      : blank
 *   1..10  : digits '0'..'9'
 *   11..15 : 'S','C','O','R','E'
 * ---------------------------------------------------------------------------- */
static const unsigned char bg_tiles[16 * 32] = {
    /* 0: blank */
    ROW(0x00), ROW(0x00), ROW(0x00), ROW(0x00),
    ROW(0x00), ROW(0x00), ROW(0x00), ROW(0x00),

    /* 1: '0' */
    ROW(0x3C), ROW(0x66), ROW(0x6E), ROW(0x76),
    ROW(0x66), ROW(0x66), ROW(0x3C), ROW(0x00),

    /* 2: '1' */
    ROW(0x18), ROW(0x38), ROW(0x18), ROW(0x18),
    ROW(0x18), ROW(0x18), ROW(0x7E), ROW(0x00),

    /* 3: '2' */
    ROW(0x3C), ROW(0x66), ROW(0x06), ROW(0x0C),
    ROW(0x30), ROW(0x60), ROW(0x7E), ROW(0x00),

    /* 4: '3' */
    ROW(0x3C), ROW(0x66), ROW(0x06), ROW(0x1C),
    ROW(0x06), ROW(0x66), ROW(0x3C), ROW(0x00),

    /* 5: '4' */
    ROW(0x0C), ROW(0x1C), ROW(0x3C), ROW(0x6C),
    ROW(0x7E), ROW(0x0C), ROW(0x0C), ROW(0x00),

    /* 6: '5' */
    ROW(0x7E), ROW(0x60), ROW(0x7C), ROW(0x06),
    ROW(0x06), ROW(0x66), ROW(0x3C), ROW(0x00),

    /* 7: '6' */
    ROW(0x1C), ROW(0x30), ROW(0x60), ROW(0x7C),
    ROW(0x66), ROW(0x66), ROW(0x3C), ROW(0x00),

    /* 8: '7' */
    ROW(0x7E), ROW(0x66), ROW(0x06), ROW(0x0C),
    ROW(0x18), ROW(0x18), ROW(0x18), ROW(0x00),

    /* 9: '8' */
    ROW(0x3C), ROW(0x66), ROW(0x66), ROW(0x3C),
    ROW(0x66), ROW(0x66), ROW(0x3C), ROW(0x00),

    /* 10: '9' */
    ROW(0x3C), ROW(0x66), ROW(0x66), ROW(0x3E),
    ROW(0x06), ROW(0x0C), ROW(0x38), ROW(0x00),

    /* 11: 'S' */
    ROW(0x3C), ROW(0x66), ROW(0x60), ROW(0x3C),
    ROW(0x06), ROW(0x66), ROW(0x3C), ROW(0x00),

    /* 12: 'C' */
    ROW(0x3C), ROW(0x66), ROW(0x60), ROW(0x60),
    ROW(0x60), ROW(0x66), ROW(0x3C), ROW(0x00),

    /* 13: 'O' */
    ROW(0x3C), ROW(0x66), ROW(0x66), ROW(0x66),
    ROW(0x66), ROW(0x66), ROW(0x3C), ROW(0x00),

    /* 14: 'R' */
    ROW(0x7C), ROW(0x66), ROW(0x66), ROW(0x7C),
    ROW(0x6C), ROW(0x66), ROW(0x66), ROW(0x00),

    /* 15: 'E' */
    ROW(0x7E), ROW(0x60), ROW(0x60), ROW(0x7C),
    ROW(0x60), ROW(0x60), ROW(0x7E), ROW(0x00)
};

/* Digit tile for value 0..9 is (1 + value); label tiles are 11..15. */
static const unsigned char label_tiles[5] = { 11, 12, 13, 14, 15 };

/* ----------------------------------------------------------------------------
 * Enemy sprite tile (8x8), loaded at VRAM tile 256 (sprite bank, tile 0)
 * ---------------------------------------------------------------------------- */
static const unsigned char enemy_tile[32] = {
    ROW(0x18), ROW(0x3C), ROW(0x7E), ROW(0xDB),
    ROW(0xFF), ROW(0x24), ROW(0x5A), ROW(0xA5)
};

/* ----------------------------------------------------------------------------
 * Game state
 * ---------------------------------------------------------------------------- */

/* Score stored as 6 decimal digits, index 0 = most significant. */
static unsigned char score_digits[6];

static int           enemy_x[NUM_ENEMIES];
static unsigned char enemy_y[NUM_ENEMIES];
static signed char   enemy_dx[NUM_ENEMIES];

/* Add 10 to the score (carry propagates from the tens digit upwards). */
static void addTenToScore(void)
{
    unsigned char i = 4;   /* tens digit */

    for (;;) {
        score_digits[i]++;
        if (score_digits[i] < 10) {
            break;
        }
        score_digits[i] = 0;
        if (i == 0) {
            break;               /* wrap at 999999 -> 000000 */
        }
        i--;
    }
}

/* Write the score digits into the top row of the background map. */
static void drawScore(void)
{
    unsigned char i;

    SMS_setNextTileatXY(SCORE_DIGITS_X, SCORE_Y);
    for (i = 0; i < 6; i++) {
        SMS_setTile((unsigned int)1 + score_digits[i]);
    }
}

/* Write the "SCORE" label into the top row (done once). */
static void drawLabel(void)
{
    unsigned char i;

    SMS_setNextTileatXY(SCORE_LABEL_X, SCORE_Y);
    for (i = 0; i < 5; i++) {
        SMS_setTile(label_tiles[i]);
    }
}

/* Initial enemy positions and speeds: two rows of four, alternating directions. */
static void initEnemies(void)
{
    unsigned char i;
    unsigned char speed;

    for (i = 0; i < NUM_ENEMIES; i++) {
        enemy_x[i] = (int)(i & 3) * 56 + (i >> 2) * 20;
        enemy_y[i] = (i < 4) ? ENEMY_ROW1_Y : ENEMY_ROW2_Y;

        speed = 1 + (i % 3);
        enemy_dx[i] = (i & 1) ? -(signed char)speed : (signed char)speed;
    }
}

/* Move enemies and bounce them off the screen edges. */
static void moveEnemies(void)
{
    unsigned char i;

    for (i = 0; i < NUM_ENEMIES; i++) {
        enemy_x[i] += enemy_dx[i];

        if (enemy_x[i] <= ENEMY_MIN_X) {
            enemy_x[i] = ENEMY_MIN_X;
            enemy_dx[i] = -enemy_dx[i];
        } else if (enemy_x[i] >= ENEMY_MAX_X) {
            enemy_x[i] = ENEMY_MAX_X;
            enemy_dx[i] = -enemy_dx[i];
        }
    }
}

/* Rebuild the sprite list for the current frame. */
static void buildSprites(void)
{
    unsigned char i;

    SMS_initSprites();
    for (i = 0; i < NUM_ENEMIES; i++) {
        SMS_addSprite(enemy_x[i], enemy_y[i], ENEMY_TILE);
    }
}

/* ----------------------------------------------------------------------------
 * Entry point
 * ---------------------------------------------------------------------------- */
void main(void)
{
    unsigned char i;

    /* Palettes */
    SMS_loadBGPalette(bg_palette);
    SMS_loadSpritePalette(spr_palette);

    /* Tiles: background at VRAM tile 0, enemy sprite at VRAM tile 256 */
    SMS_loadTiles(bg_tiles, 0, sizeof(bg_tiles));
    SMS_loadTiles(enemy_tile, 256, sizeof(enemy_tile));

    /* Clear the name table, then draw the static label */
    SMS_VRAMmemset(SMS_PNTAddress, 0, 0x700);
    drawLabel();

    /* Initial state */
    for (i = 0; i < 6; i++) {
        score_digits[i] = 0;
    }
    initEnemies();

    /* Prime the sprite table and turn the display on */
    buildSprites();
    SMS_copySpritestoSAT();
    SMS_displayOn();

    /* Main loop: one iteration per frame */
    for (;;) {
        SMS_waitForVBlank();

        /* VRAM updates during VBlank: sprites from last frame and the score */
        SMS_copySpritestoSAT();
        drawScore();

        /* Game logic for this frame */
        addTenToScore();
        moveEnemies();
        buildSprites();
    }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
