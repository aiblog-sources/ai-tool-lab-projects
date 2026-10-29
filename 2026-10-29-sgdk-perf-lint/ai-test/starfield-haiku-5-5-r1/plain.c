#include <genesis.h>

/* ------------------------------------------------------------------ */
/* Configuration                                                      */
/* ------------------------------------------------------------------ */

#define SCREEN_W        320
#define SCREEN_H        224

#define NUM_STARS       64
#define SHIP_W          16
#define SHIP_H          16
#define SHIP_SPEED      2

/* Layer ranges over the star array: far, mid, near */
#define FAR_COUNT       24          /* stars [0, 24)  : far  */
#define MID_END         48          /* stars [24, 48) : mid  */
                                    /* stars [48, 64) : near */

/* Sprite 0 is the ship (drawn on top); sprites 1..64 are the stars. */
#define SHIP_SPRITE     0

/* VRAM tile indices: 1 tile for the star, 4 tiles for the ship */
#define STAR_TILE       TILE_USER_INDEX
#define SHIP_TILE       (TILE_USER_INDEX + 1)

/* ------------------------------------------------------------------ */
/* Tile data (4bpp, 8x8 tiles, 8 u32 per tile, leftmost pixel in the  */
/* high nibble of each row word)                                      */
/* ------------------------------------------------------------------ */

/* Star: a 2x2 pixel dot using palette index 1 */
static const u32 starTile[8] =
{
    0x00000000, 0x00000000, 0x00000000, 0x00011000,
    0x00011000, 0x00000000, 0x00000000, 0x00000000
};

/*
 * Ship: 16x16 pixels made of 4 tiles.
 * Tiles are stored in the order the VDP expects for a SPRITE_SIZE(2,2)
 * sprite (column-major): top-left, bottom-left, top-right, bottom-right.
 * Palette index 1 = cyan body, 2 = yellow cockpit/engine.
 */
static const u32 shipTiles[32] =
{
    /* top-left */
    0x00000000, 0x00000000, 0x00001100, 0x00011111,
    0x00111111, 0x01111111, 0x01111111, 0x01111111,
    /* bottom-left */
    0x01111111, 0x01111111, 0x01111111, 0x00111111,
    0x00011111, 0x00001100, 0x00000000, 0x00000000,
    /* top-right */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x11000000, 0x11110000, 0x22211100, 0x22211111,
    /* bottom-right */
    0x22211111, 0x22211100, 0x11110000, 0x11000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000
};

/* ------------------------------------------------------------------ */
/* Palettes (index 0 is transparent for sprites)                      */
/* ------------------------------------------------------------------ */

static const u16 palShip[16] =
{
    RGB24_TO_VDPCOLOR(0x000000),   /* 0: transparent */
    RGB24_TO_VDPCOLOR(0x00FFFF),   /* 1: cyan body   */
    RGB24_TO_VDPCOLOR(0xFFFF00)    /* 2: yellow      */
};

/* Star brightness is chosen by palette: far = dim, near = bright */
static const u16 palFar[16] =
{
    RGB24_TO_VDPCOLOR(0x000000),
    RGB24_TO_VDPCOLOR(0x505050)
};

static const u16 palMid[16] =
{
    RGB24_TO_VDPCOLOR(0x000000),
    RGB24_TO_VDPCOLOR(0x909090)
};

static const u16 palNear[16] =
{
    RGB24_TO_VDPCOLOR(0x000000),
    RGB24_TO_VDPCOLOR(0xFFFFFF)
};

/* ------------------------------------------------------------------ */
/* Starfield                                                          */
/* ------------------------------------------------------------------ */

typedef struct
{
    s16 x;
    s16 y;
} Star;

static Star stars[NUM_STARS];

/* Small xorshift PRNG so the code does not depend on library RNG details */
static u16 rngState = 0xACE1;

static u16 rnd(void)
{
    rngState ^= (u16)(rngState << 7);
    rngState ^= (u16)(rngState >> 9);
    rngState ^= (u16)(rngState << 8);
    return rngState;
}

/*
 * Scroll stars [from, to) by 'speed' pixels and update their sprite
 * positions. Stars are processed in index order so the RNG sequence
 * matches the original per-star loop exactly.
 * Sprite attributes and links are constant and set once at startup.
 */
static void updateStars(u16 from, u16 to, s16 speed)
{
    u16 i;

    for (i = from; i < to; i++)
    {
        s16 x = stars[i].x - speed;

        if (x < 0)
        {
            x += SCREEN_W;
            stars[i].y = rnd() % SCREEN_H;
        }

        stars[i].x = x;
        VDP_setSpritePosition(i + 1, x, stars[i].y);
    }
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */

int main(bool hardReset)
{
    u16 i;
    u16 joy;
    u16 pal;
    s16 shipX = 48;
    s16 shipY = 112;

    (void)hardReset;

    JOY_init();
    SPR_init();

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    /* Palettes */
    PAL_setPalette(PAL0, palShip, CPU);
    PAL_setPalette(PAL1, palFar,  CPU);
    PAL_setPalette(PAL2, palMid,  CPU);
    PAL_setPalette(PAL3, palNear, CPU);

    /* Tiles */
    VDP_loadTileData(starTile, STAR_TILE, 1, CPU);
    VDP_loadTileData(shipTiles, SHIP_TILE, 4, CPU);

    /* Distribute stars */
    for (i = 0; i < NUM_STARS; i++)
    {
        stars[i].x = rnd() % SCREEN_W;
        stars[i].y = rnd() % SCREEN_H;
    }

    /* Star sprites: size, palette, tile and link never change, so set once */
    for (i = 0; i < NUM_STARS; i++)
    {
        if (i < FAR_COUNT)
            pal = PAL1;
        else if (i < MID_END)
            pal = PAL2;
        else
            pal = PAL3;

        VDP_setSpriteFull(
            i + 1,
            stars[i].x,
            stars[i].y,
            SPRITE_SIZE(1, 1),
            TILE_ATTR_FULL(pal, 0, FALSE, FALSE, STAR_TILE),
            (i == NUM_STARS - 1) ? 0 : (i + 2));
    }

    /* Ship sprite: linked to first star, attributes fixed */
    VDP_setSpriteFull(
        SHIP_SPRITE,
        shipX,
        shipY,
        SPRITE_SIZE(2, 2),
        TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, SHIP_TILE),
        1);

    while (TRUE)
    {
        /* ---- Input: move ship with d-pad on pad 1 ---- */
        joy = JOY_readJoypad(JOY_1);

        if (joy & BUTTON_LEFT)  shipX -= SHIP_SPEED;
        if (joy & BUTTON_RIGHT) shipX += SHIP_SPEED;
        if (joy & BUTTON_UP)    shipY -= SHIP_SPEED;
        if (joy & BUTTON_DOWN)  shipY += SHIP_SPEED;

        if (shipX < 0) shipX = 0;
        if (shipX > SCREEN_W - SHIP_W) shipX = SCREEN_W - SHIP_W;
        if (shipY < 0) shipY = 0;
        if (shipY > SCREEN_H - SHIP_H) shipY = SCREEN_H - SHIP_H;

        /* ---- Update stars, grouped by layer ---- */
        updateStars(0,          FAR_COUNT, 1);
        updateStars(FAR_COUNT,  MID_END,   2);
        updateStars(MID_END,    NUM_STARS, 3);

        /* ---- Ship position ---- */
        VDP_setSpritePosition(SHIP_SPRITE, shipX, shipY);

        /* ---- Push sprite table and wait for VBlank ---- */
        VDP_updateSprites(NUM_STARS + 1, DMA_QUEUE);
        SYS_doVBlankProcess();
    }

    return 0;
}
