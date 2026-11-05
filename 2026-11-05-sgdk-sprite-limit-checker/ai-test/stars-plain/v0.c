#include <genesis.h>

#define NUM_STARS     64
#define NUM_SPRITES   (NUM_STARS + 1)      /* sprite 0 = ship, 1..64 = stars */

#define SHIP_W        32
#define SHIP_H        24
#define SHIP_SPEED    2
#define SCREEN_W      320
#define SCREEN_H      224

/* VDP sprite coordinates have a (128,128) offset */
#define SPR_OFFSET    128

/* ---- Tile indices in VRAM ---- */
#define TILE_STAR_FAR   (TILE_USER_INDEX + 0)
#define TILE_STAR_MID   (TILE_USER_INDEX + 1)
#define TILE_STAR_NEAR  (TILE_USER_INDEX + 2)
#define TILE_SHIP       (TILE_USER_INDEX + 3)   /* 12 tiles (4x3) */

/* ---- Star tiles (8x8, 4bpp). Palette 0: 1=dim, 2=medium, 3=bright ---- */
static const u32 starTiles[3 * 8] =
{
    /* far: single dim pixel */
    0x00000000, 0x00000000, 0x00000000, 0x00010000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    /* mid: single medium pixel */
    0x00000000, 0x00000000, 0x00020000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    /* near: 2x2 bright block */
    0x00000000, 0x00000000, 0x00000000, 0x00033000,
    0x00033000, 0x00000000, 0x00000000, 0x00000000
};

/* ---- Ship art: 24 rows x 4 words (32 px wide). Top half is stored,
        bottom half is mirrored at load time. Palette 1:
        1=outline, 2=hull, 3=cockpit, 4=wing, 5=flame ---- */
static const u32 shipTopRows[12][4] =
{
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x04400000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x04440000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x04444000, 0x11000000, 0x00000000 },
    { 0x00000000, 0x02244400, 0x11111000, 0x00000000 },
    { 0x00000000, 0x22222441, 0x22211100, 0x00000000 },
    { 0x00000000, 0x12222222, 0x22222111, 0x11000000 },
    { 0x00000555, 0x12222222, 0x22223332, 0x21111000 },
    { 0x00005551, 0x12222222, 0x22233333, 0x22221110 },
    { 0x00055511, 0x12222222, 0x22222333, 0x32222221 },
    { 0x00555111, 0x11222222, 0x22222222, 0x22222222 }
};

/* Converted ship tiles (sprite tile order is column-major) */
static u32 shipTiles[12 * 8];

/* ---- Palettes ---- */
static const u16 palStars[16] =
{
    0x0000, 0x0444, 0x0888, 0x0EEE,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
};

static const u16 palShip[16] =
{
    0x0000, 0x0642, 0x0AAA, 0x0E80,
    0x002E, 0x028E, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
};

/* ---- Game state ---- */
typedef struct
{
    s16 x;
    s16 y;
    u8  speed;
} Star;

static Star stars[NUM_STARS];
static s16 shipX, shipY;

static void buildShipTiles(void)
{
    u16 tx, ty, line;
    for (tx = 0; tx < 4; tx++)
    {
        for (ty = 0; ty < 3; ty++)
        {
            for (line = 0; line < 8; line++)
            {
                u16 row = ty * 8 + line;                 /* 0..23 */
                u16 src = (row < 12) ? row : (23 - row); /* mirror bottom half */
                shipTiles[(tx * 3 + ty) * 8 + line] = shipTopRows[src][tx];
            }
        }
    }
}

static void initStars(void)
{
    u16 i;
    for (i = 0; i < NUM_STARS; i++)
    {
        /* Y spread by a permutation so that at most ~3 stars overlap on any
           scanline (well below the hardware limit of 20 per line). */
        stars[i].y     = (s16)(((i * 37) & 63) * 7 / 2);
        stars[i].x     = (s16)((i * 53) % SCREEN_W);
        stars[i].speed = (u8)(1 + (i % 3));              /* 1, 2, 3 px/frame */
    }
}

static void initSprites(void)
{
    u16 i;

    /* Ship first in the list so it is drawn on top of the stars */
    VDP_setSpriteFull(0,
                      shipX + SPR_OFFSET, shipY + SPR_OFFSET,
                      SPRITE_SIZE(4, 3),
                      TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, TILE_SHIP),
                      1);

    for (i = 0; i < NUM_STARS; i++)
    {
        u16 tile = TILE_STAR_FAR + (stars[i].speed - 1);
        VDP_setSpriteFull(1 + i,
                          stars[i].x + SPR_OFFSET, stars[i].y + SPR_OFFSET,
                          SPRITE_SIZE(1, 1),
                          TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, tile),
                          (i == NUM_STARS - 1) ? 0 : (u8)(2 + i));
    }
}

int main(bool hardReset)
{
    u16 i, joy;

    VDP_setScreenWidth320();
    VDP_setBackgroundColor(0);

    /* Palettes and tiles (all defined in code) */
    PAL_setPalette(PAL0, palStars, CPU);
    PAL_setPalette(PAL1, palShip, CPU);

    buildShipTiles();
    VDP_loadTileData(starTiles, TILE_STAR_FAR, 3, CPU);
    VDP_loadTileData(shipTiles, TILE_SHIP, 12, CPU);

    shipX = 40;
    shipY = (SCREEN_H - SHIP_H) / 2;

    initStars();
    initSprites();

    while (TRUE)
    {
        /* Input: pad 1 */
        joy = JOY_readJoypad(JOY_1);

        if (joy & BUTTON_LEFT)  shipX -= SHIP_SPEED;
        if (joy & BUTTON_RIGHT) shipX += SHIP_SPEED;
        if (joy & BUTTON_UP)    shipY -= SHIP_SPEED;
        if (joy & BUTTON_DOWN)  shipY += SHIP_SPEED;

        if (shipX < 0)                  shipX = 0;
        if (shipX > SCREEN_W - SHIP_W)  shipX = SCREEN_W - SHIP_W;
        if (shipY < 0)                  shipY = 0;
        if (shipY > SCREEN_H - SHIP_H)  shipY = SCREEN_H - SHIP_H;

        VDP_setSpritePosition(0, shipX + SPR_OFFSET, shipY + SPR_OFFSET);

        /* Starfield: move left, wrap around to the right edge */
        for (i = 0; i < NUM_STARS; i++)
        {
            stars[i].x -= stars[i].speed;
            if (stars[i].x < -8) stars[i].x += SCREEN_W + 8;

            VDP_setSpritePosition(1 + i,
                                  stars[i].x + SPR_OFFSET,
                                  stars[i].y + SPR_OFFSET);
        }

        VDP_updateSprites(NUM_SPRITES, DMA_QUEUE);
        SYS_doVBlankProcess();
    }

    return 0;
}
