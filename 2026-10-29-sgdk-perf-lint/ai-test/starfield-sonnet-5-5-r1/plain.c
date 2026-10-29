/*
 * main.c - Parallax starfield + player ship (SGDK 2.09, Sega Mega Drive)
 *
 * - 64 stars drawn as hardware sprites, scrolling left at 3 speeds (1, 2, 3 px/frame)
 * - Player ship (16x16 sprite, 2x2 tiles) moved with the pad 1 d-pad
 * - No resource files: all tiles are const u32 arrays loaded with VDP_loadTileData
 * - Uses the raw VDP sprite table (no sprite engine / SPR_*), 65 sprites total
 *
 * Performance notes:
 * - Star X position lives directly in the sprite cache (vdpSpriteCache[].x,
 *   which holds x + 0x80), so there is no separate state array to copy from.
 * - Star Y never changes after init, so it is not rewritten every frame.
 * - The per-frame star loop is a pointer-walking loop with no function calls.
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

/* Sprite coordinates in the VDP table are offset by 0x80 */
#define SPR_OFFS        0x80
#define STAR_WRAP_MIN   (SPR_OFFS - 8)              /* fully off the left edge */
#define STAR_WRAP_ADD   (SCREEN_W + 8)

/* ------------------------------------------------------------------------- */
/* Tile data (8 u32 per 8x8 tile, one nibble per pixel = palette color index) */
/* ------------------------------------------------------------------------- */

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

static u16 starSpeed[NUM_STARS];

static s16 shipX;
static s16 shipY;

/* ------------------------------------------------------------------------- */
/* Init helpers                                                               */
/* ------------------------------------------------------------------------- */

static void initPalettes(void)
{
    PAL_setColor(0, 0x0000);

    PAL_setColor(1, 0x0444);
    PAL_setColor(2, 0x0888);
    PAL_setColor(3, 0x0EEE);

    PAL_setColor(16 + 1, 0x0A86);
    PAL_setColor(16 + 2, 0x0EEA);
    PAL_setColor(16 + 3, 0x0EE0);
    PAL_setColor(16 + 4, 0x006E);
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

    for (i = 0; i < NUM_STARS; i++)
    {
        u16 idx   = i + 1;
        u16 speed = (i % 3) + 1;                    /* 1, 2 or 3 px/frame */
        /*
         * Y positions are a permutation of evenly spaced rows (37 is coprime
         * with 64), so stars never pile up on one scanline.
         */
        s16 y = (s16) ((((i * 37) & 63) * 7) / 2);
        s16 x = (s16) (random() % SCREEN_W);
        u8  link = (idx == NUM_SPRITES - 1) ? 0 : (u8) (idx + 1);

        starSpeed[i] = speed;

        VDP_setSpriteFull(idx, x, y,
                          SPRITE_SIZE(1, 1),
                          TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_STAR_BASE + speed - 1),
                          link);
    }
}

/* ------------------------------------------------------------------------- */
/* Per-frame updates                                                          */
/* ------------------------------------------------------------------------- */

static void updateShip(void)
{
    u16 joy = JOY_readJoypad(JOY_1);
    s16 x = shipX;
    s16 y = shipY;

    if (joy & BUTTON_LEFT)  x -= SHIP_SPEED;
    if (joy & BUTTON_RIGHT) x += SHIP_SPEED;
    if (joy & BUTTON_UP)    y -= SHIP_SPEED;
    if (joy & BUTTON_DOWN)  y += SHIP_SPEED;

    if (x < 0)                      x = 0;
    if (x > SCREEN_W - SHIP_SIZE)   x = SCREEN_W - SHIP_SIZE;
    if (y < 0)                      y = 0;
    if (y > SCREEN_H - SHIP_SIZE)   y = SCREEN_H - SHIP_SIZE;

    shipX = x;
    shipY = y;

    vdpSpriteCache[0].x = x + SPR_OFFS;
    vdpSpriteCache[0].y = y + SPR_OFFS;
}

static void updateStars(void)
{
    VDPSprite *spr = &vdpSpriteCache[1];
    const u16 *sp = starSpeed;
    u16 n = NUM_STARS - 1;

    do
    {
        u16 x = spr->x - *sp++;

        if (x < STAR_WRAP_MIN)
            x += STAR_WRAP_ADD;

        spr->x = x;
        spr++;
    }
    while (n--);
}

/* ------------------------------------------------------------------------- */
/* Entry point                                                                */
/* ------------------------------------------------------------------------- */

int main(bool hardReset)
{
    (void) hardReset;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    JOY_init();
    setRandomSeed(0x1234);

    initPalettes();

    VDP_loadTileData(starTiles, TILE_STAR_BASE, 3, CPU);
    VDP_loadTileData(shipTiles, TILE_SHIP_BASE, 4, CPU);

    shipX = 40;
    shipY = (SCREEN_H - SHIP_SIZE) / 2;

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
