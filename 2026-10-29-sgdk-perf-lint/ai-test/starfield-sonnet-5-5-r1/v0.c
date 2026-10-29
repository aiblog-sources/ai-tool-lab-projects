/*
 * main.c - Parallax starfield + player ship (SGDK 2.09, Sega Mega Drive)
 *
 * - 64 stars drawn as hardware sprites, scrolling left at 3 speeds (1, 2, 3 px/frame)
 * - Player ship (16x16 sprite, 2x2 tiles) moved with the pad 1 d-pad
 * - No resource files: all tiles are const u32 arrays loaded with VDP_loadTileData
 * - Uses the raw VDP sprite table (no sprite engine / SPR_*), 65 sprites total
 */

#include <genesis.h>

#define NUM_STARS       64
#define NUM_SPRITES     (NUM_STARS + 1)     /* sprite 0 = ship, 1..64 = stars */

#define SCREEN_W        320
#define SCREEN_H        224

#define SHIP_SIZE       16
#define SHIP_SPEED      3

/* Tile layout in VRAM */
#define TILE_STAR_BASE  (TILE_USER_INDEX)           /* 3 star tiles  */
#define TILE_SHIP_BASE  (TILE_USER_INDEX + 3)       /* 4 ship tiles  */

/* ------------------------------------------------------------------------- */
/* Tile data (8 u32 per 8x8 tile, one nibble per pixel = palette color index) */
/* ------------------------------------------------------------------------- */

/* Star tiles: 0 = far/dim (color 1), 1 = middle (color 2), 2 = near/bright 2x2 (color 3) */
static const u32 starTiles[3 * 8] =
{
    /* far: single dim pixel */
    0x00000000, 0x00000000, 0x00000000, 0x00010000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,

    /* middle: single medium pixel */
    0x00000000, 0x00000000, 0x00000000, 0x00020000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,

    /* near: 2x2 bright block */
    0x00000000, 0x00000000, 0x00000000, 0x00330000,
    0x00330000, 0x00000000, 0x00000000, 0x00000000
};

/*
 * Ship: 16x16 = 2x2 tiles. Hardware sprites order multi-tile sprites
 * column by column: top-left, bottom-left, top-right, bottom-right.
 * Colors (palette 1): 1 = hull, 2 = light hull, 3 = cockpit, 4 = engine flame.
 */
static const u32 shipTiles[4 * 8] =
{
    /* top-left */
    0x00000000, 0x00110000, 0x00111100, 0x00111111,
    0x00111122, 0x44111122, 0x44411122, 0x44441112,

    /* bottom-left */
    0x44441112, 0x44411122, 0x44111122, 0x00111122,
    0x00111111, 0x00111100, 0x00110000, 0x00000000,

    /* top-right */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x22000000, 0x22220000, 0x23322200, 0x23333222,

    /* bottom-right */
    0x23333222, 0x23322200, 0x22220000, 0x22000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000
};

/* ------------------------------------------------------------------------- */
/* State                                                                      */
/* ------------------------------------------------------------------------- */

static s16 starX[NUM_STARS];
static s16 starY[NUM_STARS];
static u8  starSpeed[NUM_STARS];

static s16 shipX;
static s16 shipY;

/* ------------------------------------------------------------------------- */
/* Init helpers                                                               */
/* ------------------------------------------------------------------------- */

static void initPalettes(void)
{
    /* Background (also transparent color for sprites) */
    PAL_setColor(0, 0x0000);

    /* PAL0: star shades (VDP color format 0x0BGR) */
    PAL_setColor(1, 0x0444);    /* dim   */
    PAL_setColor(2, 0x0888);    /* mid   */
    PAL_setColor(3, 0x0EEE);    /* white */

    /* PAL1: ship (index = 16 * palette + color) */
    PAL_setColor(16 + 1, 0x0A86);   /* hull       */
    PAL_setColor(16 + 2, 0x0EEA);   /* light hull */
    PAL_setColor(16 + 3, 0x0EE0);   /* cockpit    */
    PAL_setColor(16 + 4, 0x006E);   /* flame      */
}

static void initStars(void)
{
    u16 i;

    for (i = 0; i < NUM_STARS; i++)
    {
        /*
         * Y positions are a permutation of evenly spaced rows (37 is coprime
         * with 64), so stars are spread over the whole screen and never pile
         * up on one scanline (hardware limit: 20 sprites per line).
         */
        starY[i] = (s16) ((((i * 37) & 63) * 7) / 2);
        starX[i] = (s16) (random() % SCREEN_W);
        starSpeed[i] = (u8) ((i % 3) + 1);          /* 1, 2 or 3 px/frame */
    }
}

static void initSprites(void)
{
    u16 i;

    VDP_resetSprites();

    /* Sprite 0: ship (drawn on top of stars, lowest index = highest priority) */
    VDP_setSpriteFull(0, shipX, shipY,
                      SPRITE_SIZE(2, 2),
                      TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, TILE_SHIP_BASE),
                      1);

    /* Sprites 1..64: stars, tile chosen by speed layer */
    for (i = 0; i < NUM_STARS; i++)
    {
        u16 idx   = i + 1;
        u16 layer = starSpeed[i] - 1;
        u8  link  = (idx == NUM_SPRITES - 1) ? 0 : (u8) (idx + 1);

        VDP_setSpriteFull(idx, starX[i], starY[i],
                          SPRITE_SIZE(1, 1),
                          TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_STAR_BASE + layer),
                          link);
    }
}

/* ------------------------------------------------------------------------- */
/* Per-frame updates                                                          */
/* ------------------------------------------------------------------------- */

static void updateShip(void)
{
    u16 joy = JOY_readJoypad(JOY_1);

    if (joy & BUTTON_LEFT)  shipX -= SHIP_SPEED;
    if (joy & BUTTON_RIGHT) shipX += SHIP_SPEED;
    if (joy & BUTTON_UP)    shipY -= SHIP_SPEED;
    if (joy & BUTTON_DOWN)  shipY += SHIP_SPEED;

    if (shipX < 0)                      shipX = 0;
    if (shipX > SCREEN_W - SHIP_SIZE)   shipX = SCREEN_W - SHIP_SIZE;
    if (shipY < 0)                      shipY = 0;
    if (shipY > SCREEN_H - SHIP_SIZE)   shipY = SCREEN_H - SHIP_SIZE;

    VDP_setSpritePosition(0, shipX, shipY);
}

static void updateStars(void)
{
    u16 i;

    for (i = 0; i < NUM_STARS; i++)
    {
        starX[i] -= starSpeed[i];

        /* Fully off the left edge (sprite is 8 px wide): wrap to the right side */
        if (starX[i] < -8)
            starX[i] += SCREEN_W + 8;

        VDP_setSpritePosition(i + 1, starX[i], starY[i]);
    }
}

/* ------------------------------------------------------------------------- */
/* Entry point                                                                */
/* ------------------------------------------------------------------------- */

int main(bool hardReset)
{
    (void) hardReset;

    /* 320x224 mode */
    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    JOY_init();
    setRandomSeed(0x1234);

    initPalettes();

    VDP_loadTileData(starTiles, TILE_STAR_BASE, 3, CPU);
    VDP_loadTileData(shipTiles, TILE_SHIP_BASE, 4, CPU);

    shipX = 40;
    shipY = (SCREEN_H - SHIP_SIZE) / 2;

    initStars();
    initSprites();

    while (TRUE)
    {
        updateShip();
        updateStars();

        VDP_updateSprites(NUM_SPRITES, DMA_QUEUE);
        SYS_doVBlankProcess();
    }

    return 0;
}
