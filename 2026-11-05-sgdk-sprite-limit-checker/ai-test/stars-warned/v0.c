/*
 * main.c - SGDK 2.11
 * Parallax starfield (64 hardware sprites, 8x8, 3 speeds) + 32x24 player ship.
 *
 * Hardware budget (H40 / 320x224):
 *   - Sprites in use ........ 65 of 80 max
 *   - Sprites per scanline .. at most 4 (ship + 3 stars) of 20 max
 *   - Sprite pixels per line . at most 4*8 = 32.. well under the 320 limit
 *
 * Why no dropout/flicker: stars never change Y, and every star gets a unique
 * Y "slot" spaced 3-4 px apart (64 slots over 224 lines). An 8px-tall sprite
 * therefore overlaps at most 3 other stars on any scanline. Stars only move
 * horizontally, so the per-line load is constant for the whole run.
 * The sprite table is sent to VRAM through the DMA queue during VBlank, so
 * there is no mid-frame tearing.
 *
 * No resource files: all tiles are const u32 arrays loaded with VDP_loadTileData.
 */

#include <genesis.h>

#define NUM_STARS       64
#define NUM_SPRITES     (NUM_STARS + 1)     /* sprite 0 = ship, 1..64 = stars */

#define SCREEN_W        320
#define SCREEN_H        224

#define SHIP_W          32
#define SHIP_H          24
#define SHIP_SPEED      3

#define TILE_STAR_BASE  (TILE_USER_INDEX)           /* 3 star tiles  */
#define TILE_SHIP_BASE  (TILE_USER_INDEX + 3)       /* 12 ship tiles */

/* ------------------------------------------------------------------------- */
/* Tile data (4bpp, 8 u32 per 8x8 tile, one nibble per pixel)                */
/* ------------------------------------------------------------------------- */

/* 3 star tiles: far (dim dot), mid (2px streak), near (3px bright streak) */
static const u32 starTiles[3 * 8] =
{
    /* far: colour 1 */
    0x00000000, 0x00000000, 0x00000000, 0x00001000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    /* mid: colour 2 */
    0x00000000, 0x00000000, 0x00000000, 0x00022000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    /* near: colour 3 with dim tail */
    0x00000000, 0x00000000, 0x00000000, 0x02333000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000
};

/*
 * Ship: 32x24 = 4 columns x 3 rows of tiles.
 * Hardware sprites order tiles COLUMN-major (top to bottom, then next column).
 */
static const u32 shipTiles[12 * 8] =
{
    /* column 0, row 0 */
    0x00000000, 0x00000000, 0x00110000, 0x00177100,
    0x00177731, 0x00177733, 0x00177733, 0x00177733,
    /* column 0, row 1 */
    0x00177733, 0x06177733, 0x56177733, 0x56177733,
    0x56177733, 0x56177733, 0x06177733, 0x00177733,
    /* column 0, row 2 */
    0x00177733, 0x00177733, 0x00177733, 0x00177731,
    0x00177100, 0x00110000, 0x00000000, 0x00000000,

    /* column 1, row 0 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x31000000, 0x33310000, 0x33333100,
    /* column 1, row 1 */
    0x33333331, 0x33333333, 0x33333333, 0x33333334,
    0x33333334, 0x33333333, 0x33333333, 0x33333331,
    /* column 1, row 2 */
    0x33333100, 0x33310000, 0x31000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,

    /* column 2, row 0 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    /* column 2, row 1 */
    0x11111110, 0x22844421, 0x84444444, 0x44444444,
    0x44444444, 0x84444444, 0x22844421, 0x11111110,
    /* column 2, row 2 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,

    /* column 3, row 0 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    /* column 3, row 1 */
    0x00000000, 0x11100000, 0x22211100, 0x42222211,
    0x42222211, 0x22211100, 0x11100000, 0x00000000,
    /* column 3, row 2 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000
};

/* ------------------------------------------------------------------------- */
/* Palettes (0x0BGR, 3 bits per channel, even values)                        */
/* ------------------------------------------------------------------------- */

static const u16 starPalette[16] =
{
    0x0000,     /* 0: transparent / black */
    0x0444,     /* 1: far stars, dim grey */
    0x0A88,     /* 2: mid stars, light blue-grey */
    0x0EEE      /* 3: near stars, white */
};

static const u16 shipPalette[16] =
{
    0x0000,     /* 0: transparent */
    0x0420,     /* 1: outline, dark navy */
    0x0CCC,     /* 2: hull light */
    0x0A86,     /* 3: hull mid */
    0x0EE2,     /* 4: cockpit cyan */
    0x006E,     /* 5: flame orange */
    0x00EE,     /* 6: flame yellow */
    0x022C,     /* 7: red wing */
    0x0EEE      /* 8: highlight white */
};

/* ------------------------------------------------------------------------- */
/* Game state                                                                */
/* ------------------------------------------------------------------------- */

typedef struct
{
    s16 x;
    s16 y;          /* fixed for the lifetime of the star */
    s16 speed;      /* pixels per frame */
    u16 attr;       /* sprite tile attribute */
} Star;

static Star stars[NUM_STARS];
static s16 shipX;
static s16 shipY;

static const s16 layerSpeed[3] = { 1, 2, 4 };

/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */

static u16 rngState = 0xACE1;

static u16 nextRandom(void)
{
    rngState = (u16)(rngState * 25173u + 13849u);
    return rngState;
}

static void initStars(void)
{
    u16 i;

    for (i = 0; i < NUM_STARS; i++)
    {
        const u16 layer = i % 3;
        /* i*37 mod 64 is a permutation of 0..63 (37 is odd), so every star
           gets its own Y slot. Slots are 3.5px apart -> max 3 stars/scanline. */
        const u16 slot = (u16)((i * 37) & 63);

        stars[i].x     = (s16)((nextRandom() >> 2) % SCREEN_W);
        stars[i].y     = (s16)((slot * 7) >> 1);
        stars[i].speed = layerSpeed[layer];
        stars[i].attr  = TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_STAR_BASE + layer);
    }
}

static void updateStars(void)
{
    u16 i;

    for (i = 0; i < NUM_STARS; i++)
    {
        stars[i].x -= stars[i].speed;
        if (stars[i].x < -8)
            stars[i].x += (SCREEN_W + 8);   /* wrap back in from the right */
    }
}

static void updateShip(void)
{
    const u16 joy = JOY_readJoypad(JOY_1);

    if (joy & BUTTON_LEFT)  shipX -= SHIP_SPEED;
    if (joy & BUTTON_RIGHT) shipX += SHIP_SPEED;
    if (joy & BUTTON_UP)    shipY -= SHIP_SPEED;
    if (joy & BUTTON_DOWN)  shipY += SHIP_SPEED;

    if (shipX < 0)                  shipX = 0;
    if (shipX > SCREEN_W - SHIP_W)  shipX = SCREEN_W - SHIP_W;
    if (shipY < 0)                  shipY = 0;
    if (shipY > SCREEN_H - SHIP_H)  shipY = SCREEN_H - SHIP_H;
}

static void buildSpriteTable(void)
{
    const u16 shipAttr = TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, TILE_SHIP_BASE);
    u16 i;

    /* Sprite 0 is the ship: lowest index = drawn on top of the stars. */
    VDP_setSpriteFull(0, shipX, shipY, SPRITE_SIZE(4, 3), shipAttr, 1);

    /* Sprites 1..64 are the stars. Link chain is 0 -> 1 -> ... -> 64 -> end. */
    for (i = 0; i < NUM_STARS; i++)
    {
        const u16 idx  = i + 1;
        const u8  link = (idx == NUM_SPRITES - 1) ? 0 : (u8)(idx + 1);

        VDP_setSpriteFull(idx, stars[i].x, stars[i].y,
                          SPRITE_SIZE(1, 1), stars[i].attr, link);
    }
}

/* ------------------------------------------------------------------------- */
/* Main                                                                      */
/* ------------------------------------------------------------------------- */

int main(bool hardReset)
{
    (void)hardReset;

    /* Video mode: 320x224 (H40, V28) */
    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    /* Palettes: PAL0 = stars/background, PAL1 = ship */
    PAL_setColors(0,  starPalette, 16, CPU);
    PAL_setColors(16, shipPalette, 16, CPU);
    VDP_setBackgroundColor(0);

    /* Tiles */
    VDP_loadTileData(starTiles, TILE_STAR_BASE, 3,  CPU);
    VDP_loadTileData(shipTiles, TILE_SHIP_BASE, 12, CPU);

    /* Sprites */
    VDP_resetSprites();

    shipX = (SCREEN_W - SHIP_W) / 2;
    shipY = (SCREEN_H - SHIP_H) / 2;
    initStars();

    while (TRUE)
    {
        updateShip();
        updateStars();
        buildSpriteTable();

        /* Queue the sprite table for DMA, sent during VBlank (no tearing). */
        VDP_updateSprites(NUM_SPRITES, DMA_QUEUE);

        SYS_doVBlankProcess();
    }

    return 0;
}
