/*
 * main.c - Parallax starfield + player ship for Sega Mega Drive (SGDK 2.09)
 *
 * - 64 stars drawn as hardware sprites, scrolling left at 3 speeds (parallax)
 * - Player ship (16x16 hardware sprite) moved with the D-pad on pad 1
 * - No resource files: all tiles are const u32 arrays loaded with VDP_loadTileData
 *
 * Sprite layout in the VDP sprite table:
 *   index 0      : player ship (lowest index = drawn on top)
 *   index 1..64  : stars
 */

#include <genesis.h>

/* ------------------------------------------------------------------------- */
/* Constants                                                                 */
/* ------------------------------------------------------------------------- */

#define SCREEN_W            320
#define SCREEN_H            224

#define NUM_STARS           64
#define NUM_LAYERS          3
#define NUM_SPRITES         (1 + NUM_STARS)

#define SHIP_W              16
#define SHIP_H              16
#define SHIP_SPEED          2

/*
 * Star X is stored in quarter pixels, pre-offset by the hardware +128.
 * (All layer speeds are multiples of 1/4 px, and the extra sub-pixel bits of
 * the original 8.8 format never influenced the displayed pixel or respawn.)
 */
#define Q_SHIFT             2
#define Q_HW_OFFSET         (0x80 << Q_SHIFT)                 /* 512  */
#define Q_RESPAWN_LIMIT     ((-8 << Q_SHIFT) + Q_HW_OFFSET)   /* 480  */
#define Q_RESPAWN_BASE      ((SCREEN_W << Q_SHIFT) + Q_HW_OFFSET)

/* VRAM tile indices */
#define TILE_STAR_BASE      (TILE_USER_INDEX)       /* 3 tiles: far, mid, near */
#define TILE_SHIP_BASE      (TILE_USER_INDEX + 3)   /* 4 tiles: 2x2 ship        */

/* ------------------------------------------------------------------------- */
/* Tile data (4bpp, 8 rows of 8 pixels per tile, leftmost pixel = high nibble) */
/* ------------------------------------------------------------------------- */

static const u32 starTiles[3 * 8] =
{
    /* far star (color 1) */
    0x00000000,
    0x00000000,
    0x00000000,
    0x00010000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,

    /* mid star (color 2) */
    0x00000000,
    0x00000000,
    0x00000000,
    0x00020000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,

    /* near star (color 3), 2x2 pixels */
    0x00000000,
    0x00000000,
    0x00000000,
    0x00033000,
    0x00033000,
    0x00000000,
    0x00000000,
    0x00000000
};

static const u32 shipTiles[4 * 8] =
{
    /* top-left */
    0x00000000,
    0x00000000,
    0x44000000,
    0x44400000,
    0x04555000,
    0x04555556,
    0x74555555,
    0x77455555,

    /* bottom-left */
    0x77455555,
    0x74555555,
    0x04555556,
    0x04555000,
    0x44400000,
    0x44000000,
    0x00000000,
    0x00000000,

    /* top-right */
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x60000000,
    0x56666000,
    0x55555666,

    /* bottom-right */
    0x55555666,
    0x56666000,
    0x60000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000
};

/* ------------------------------------------------------------------------- */
/* Star state                                                                */
/* ------------------------------------------------------------------------- */

/* Per-layer speed in quarter pixels per frame: 0.50, 1.25, 2.50 px/frame */
static const s16 layerSpeedQ[NUM_LAYERS] = { 2, 5, 10 };

static s16 starQ[NUM_STARS];        /* X in 1/4 px, +128 hardware offset */
static s16 starSpeed[NUM_STARS];    /* per-star speed in 1/4 px          */

/* Ship state */
static s16 shipX;
static s16 shipY;

/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */

/* Random value in [0, range) without division: (rand16 * range) >> 16 */
static inline u16 randRange(u16 range)
{
    return (u16)(((u32)(u16)random() * (u32)(u16)range) >> 16);
}

static void initPalette(void)
{
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x000000));
    PAL_setColor(1, RGB24_TO_VDPCOLOR(0x404040));
    PAL_setColor(2, RGB24_TO_VDPCOLOR(0x909090));
    PAL_setColor(3, RGB24_TO_VDPCOLOR(0xFFFFFF));
    PAL_setColor(4, RGB24_TO_VDPCOLOR(0x2040A0));
    PAL_setColor(5, RGB24_TO_VDPCOLOR(0xC0C0C0));
    PAL_setColor(6, RGB24_TO_VDPCOLOR(0x00E0E0));
    PAL_setColor(7, RGB24_TO_VDPCOLOR(0xFF8000));
}

static void initSprites(void)
{
    u16 i;

    shipX = 40;
    shipY = (SCREEN_H - SHIP_H) / 2;

    VDP_setSpriteFull(0, shipX, shipY,
                      SPRITE_SIZE(2, 2),
                      TILE_ATTR_FULL(PAL0, TRUE, FALSE, FALSE, TILE_SHIP_BASE),
                      1);

    for (i = 0; i < NUM_STARS; i++)
    {
        const u16 spr   = i + 1;
        const u8  layer = (u8)(i % NUM_LAYERS);
        const s16 x     = (s16)randRange(SCREEN_W);
        const s16 y     = (s16)randRange(SCREEN_H - 8);
        const u8  link  = (spr == (NUM_SPRITES - 1)) ? 0 : (u8)(spr + 1);

        starSpeed[i] = layerSpeedQ[layer];
        starQ[i]     = (s16)((x << Q_SHIFT) + Q_HW_OFFSET);

        VDP_setSpriteFull(spr, x, y,
                          SPRITE_SIZE(1, 1),
                          TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE,
                                         TILE_STAR_BASE + layer),
                          link);
    }
}

static void updateShip(void)
{
    const u16 pad = JOY_readJoypad(JOY_1);

    if (pad & BUTTON_LEFT)  shipX -= SHIP_SPEED;
    if (pad & BUTTON_RIGHT) shipX += SHIP_SPEED;
    if (pad & BUTTON_UP)    shipY -= SHIP_SPEED;
    if (pad & BUTTON_DOWN)  shipY += SHIP_SPEED;

    if (shipX < 0)                      shipX = 0;
    if (shipX > (SCREEN_W - SHIP_W))    shipX = SCREEN_W - SHIP_W;
    if (shipY < 0)                      shipY = 0;
    if (shipY > (SCREEN_H - SHIP_H))    shipY = SCREEN_H - SHIP_H;

    vdpSpriteCache[0].x = shipX + 0x80;
    vdpSpriteCache[0].y = shipY + 0x80;
}

static void updateStars(void)
{
    VDPSprite *spr     = &vdpSpriteCache[1];
    s16 *pq            = starQ;
    const s16 *ps      = starSpeed;
    u16 i              = NUM_STARS;

    do
    {
        s16 q = *pq - *ps++;

        /* Fully off the left edge: respawn on the right with a new Y */
        if (q < Q_RESPAWN_LIMIT)
        {
            /* == FP(320) + randRange(FP(16)), reduced to 1/4 px */
            q = (s16)(Q_RESPAWN_BASE + (random() >> 10));
            spr->y = (s16)randRange(SCREEN_H - 8) + 0x80;
        }

        *pq++  = q;
        spr->x = q >> Q_SHIFT;
        spr++;
    } while (--i);
}

/* ------------------------------------------------------------------------- */
/* Main                                                                      */
/* ------------------------------------------------------------------------- */

int main(bool hardReset)
{
    (void)hardReset;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    PAL_setColors(0, palette_black, 64, CPU);

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    VDP_loadTileData(starTiles, TILE_STAR_BASE, 3, CPU);
    VDP_loadTileData(shipTiles, TILE_SHIP_BASE, 4, CPU);

    JOY_init();

    initPalette();
    initSprites();
    VDP_updateSprites(NUM_SPRITES, DMA_QUEUE);
    SYS_doVBlankProcess();

    while (TRUE)
    {
        updateShip();
        updateStars();

        VDP_updateSprites(NUM_SPRITES, DMA_QUEUE);

        SYS_doVBlankProcess();
    }

    return 0;
}
