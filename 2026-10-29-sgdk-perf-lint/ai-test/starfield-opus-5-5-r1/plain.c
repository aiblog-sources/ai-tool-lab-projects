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
 * Original design uses 8.8 fixed point on X. All speeds and the respawn
 * threshold are multiples of 0x40, so the low 6 fractional bits never affect
 * the displayed pixel or the respawn test. We therefore store X in 1/4-pixel
 * units (8.8 >> 6) in a 16-bit value: identical output, much cheaper on 68000.
 */
#define FP_SHIFT            8
#define FP(x)               ((s32)(x) << FP_SHIFT)

#define QP_SHIFT            2                       /* quarter-pixel units */
#define QP(x)               ((s16)((x) << QP_SHIFT))
#define FP_TO_QP_SHIFT      (FP_SHIFT - QP_SHIFT)   /* 6 */

/* VRAM tile indices */
#define TILE_STAR_BASE      (TILE_USER_INDEX)       /* 3 tiles: far, mid, near */
#define TILE_SHIP_BASE      (TILE_USER_INDEX + 3)   /* 4 tiles: 2x2 ship        */

/* ------------------------------------------------------------------------- */
/* Tile data (4bpp, 8 rows of 8 pixels per tile, leftmost pixel = high nibble) */
/* ------------------------------------------------------------------------- */

/* Star tiles: far = dim 1px, mid = grey 1px, near = bright 2x2 */
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

/*
 * Ship 16x16, facing right. Colors:
 *   4 = dark blue outline, 5 = light grey hull, 6 = cyan nose, 7 = orange engine
 *
 * Hardware sprite tiles are ordered column-major:
 *   tile 0 = top-left, tile 1 = bottom-left, tile 2 = top-right, tile 3 = bottom-right
 */
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

/* Per-layer speed in quarter-pixels per frame (== 8.8 values 0x80/0x140/0x280 >> 6) */
static const s16 layerSpeed[NUM_LAYERS] =
{
    0x0080 >> FP_TO_QP_SHIFT,   /* far : 0.50 px/frame */
    0x0140 >> FP_TO_QP_SHIFT,   /* mid : 1.25 px/frame */
    0x0280 >> FP_TO_QP_SHIFT    /* near: 2.50 px/frame */
};

static s16 starX[NUM_STARS];        /* quarter-pixel units */
static s16 starSpeed[NUM_STARS];    /* quarter-pixels per frame */

/* Ship state */
static s16 shipX;
static s16 shipY;

/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */

/* Random value in [0, range) without division: (rand16 * range) >> 16 */
static inline u16 randRange(u16 range)
{
    return (u16)(((u32)random() * (u32)range) >> 16);
}

static void initPalette(void)
{
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x000000));   /* background: black   */
    PAL_setColor(1, RGB24_TO_VDPCOLOR(0x404040));   /* far star            */
    PAL_setColor(2, RGB24_TO_VDPCOLOR(0x909090));   /* mid star            */
    PAL_setColor(3, RGB24_TO_VDPCOLOR(0xFFFFFF));   /* near star           */
    PAL_setColor(4, RGB24_TO_VDPCOLOR(0x2040A0));   /* ship outline        */
    PAL_setColor(5, RGB24_TO_VDPCOLOR(0xC0C0C0));   /* ship hull           */
    PAL_setColor(6, RGB24_TO_VDPCOLOR(0x00E0E0));   /* ship nose           */
    PAL_setColor(7, RGB24_TO_VDPCOLOR(0xFF8000));   /* ship engine         */
}

static void initSprites(void)
{
    u16 i;
    u16 layer = 0;

    /* Ship: sprite 0, 2x2 tiles, high priority so it's always on top */
    shipX = 40;
    shipY = (SCREEN_H - SHIP_H) / 2;

    VDP_setSpriteFull(0, shipX, shipY,
                      SPRITE_SIZE(2, 2),
                      TILE_ATTR_FULL(PAL0, TRUE, FALSE, FALSE, TILE_SHIP_BASE),
                      1);

    /* Stars: sprites 1..64, 1x1 tile each */
    for (i = 0; i < NUM_STARS; i++)
    {
        const u16 spr  = i + 1;
        const s16 x    = (s16)randRange(SCREEN_W);
        const s16 y    = (s16)randRange(SCREEN_H - 8);
        const u8  link = (spr == (NUM_SPRITES - 1)) ? 0 : (u8)(spr + 1);

        starSpeed[i] = layerSpeed[layer];
        starX[i]     = QP(x);

        VDP_setSpriteFull(spr, x, y,
                          SPRITE_SIZE(1, 1),
                          TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE,
                                         TILE_STAR_BASE + layer),
                          link);

        /* layer = i % NUM_LAYERS without division */
        if (++layer == NUM_LAYERS) layer = 0;
    }
}

static void updateShip(void)
{
    const u16 pad = JOY_readJoypad(JOY_1);
    s16 x = shipX;
    s16 y = shipY;

    if (pad & BUTTON_LEFT)  x -= SHIP_SPEED;
    if (pad & BUTTON_RIGHT) x += SHIP_SPEED;
    if (pad & BUTTON_UP)    y -= SHIP_SPEED;
    if (pad & BUTTON_DOWN)  y += SHIP_SPEED;

    if (x < 0)                      x = 0;
    if (x > (SCREEN_W - SHIP_W))    x = SCREEN_W - SHIP_W;
    if (y < 0)                      y = 0;
    if (y > (SCREEN_H - SHIP_H))    y = SCREEN_H - SHIP_H;

    shipX = x;
    shipY = y;

    /* Write directly into SGDK's sprite cache (hardware coords are +128) */
    vdpSpriteCache[0].x = x + 0x80;
    vdpSpriteCache[0].y = y + 0x80;
}

static void updateStars(void)
{
    VDPSprite *spr   = &vdpSpriteCache[1];
    s16 *px          = starX;
    const s16 *ps    = starSpeed;
    s16 i            = NUM_STARS - 1;

    do
    {
        s16 x = *px - *ps++;

        /* Fully off the left edge: respawn on the right with a new Y */
        if (x < QP(-8))
        {
            x = QP(SCREEN_W) + (s16)(randRange(FP(16)) >> FP_TO_QP_SHIFT);
            spr->y = (s16)randRange(SCREEN_H - 8) + 0x80;
        }

        *px++  = x;
        spr->x = (x >> QP_SHIFT) + 0x80;
        spr++;
    } while (--i >= 0);
}

/* ------------------------------------------------------------------------- */
/* Main                                                                      */
/* ------------------------------------------------------------------------- */

int main(bool hardReset)
{
    (void)hardReset;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    /* Blank everything while setting up */
    PAL_setColors(0, palette_black, 64, CPU);

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    /* Load tiles into VRAM */
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

        /* Queue sprite table upload; DMA happens during VBlank */
        VDP_updateSprites(NUM_SPRITES, DMA_QUEUE);

        SYS_doVBlankProcess();
    }

    return 0;
}
