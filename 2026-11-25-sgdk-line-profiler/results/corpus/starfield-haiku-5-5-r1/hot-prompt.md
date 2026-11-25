Here is a Mega Drive game written with SGDK 2.09. Make it run faster on the real hardware (68000 at 7.67 MHz) without changing what the game does or how it looks. Reply with the full main.c in one ```c code block.

Below is a line profile of this code, measured in an emulator with a scripted pad: cycles per video frame by source line (one NTSC frame is 128,005 cycles), what each line called, and the findings of a performance lint on the same lines.

== Lines by measured cost (sgdkhot: mdlines + sgdklint) ==
budget 128,005 cycles per video frame (NTSC); 1,200 frames counted; busy 25,639 cycles per frame on average (20.0%)
each line = its own instructions + every library/libgcc call made from it (game helpers count on their own lines)
    7,548 /frame  29.4% of busy  src/main.c:187  VDP_setSpriteFull(
    4,820 /frame  18.8% of busy  src/main.c:207  SYS_doVBlankProcess();
                     4,773 in SYS_doVBlankProcessEx.constprop.0.isra.0: 1.00 calls/frame, 4,773 cycles each
    4,032 /frame  15.7% of busy  src/main.c:178  s->x -= layerSpeed[s->layer];
    3,713 /frame  14.5% of busy  src/main.c:192  TILE_ATTR_FULL(layerPal[s->layer], 0, FALSE, FALSE, STAR_TIL
    2,243 /frame   8.7% of busy  src/main.c:174  for (i = 0; i < NUM_STARS; i++)
      977 /frame   3.8% of busy  src/main.c:206  VDP_updateSprites(NUM_STARS + 1, DMA_QUEUE);
                       794 in DMA_queueDmaFast: 1.00 calls/frame, 794 cycles each
      768 /frame   3.0% of busy  src/main.c:180  if (s->x < 0)
      535 /frame   2.1% of busy  src/main.c:193  (i == NUM_STARS - 1) ? 0 : (i + 2));
      318 /frame   1.2% of busy  [no game line]:0  
                       125 in no_bmp_task
                       124 in no_user_task
                        30 in no_xgm_task
      256 /frame   1.0% of busy  src/main.c:188  i + 1,
      102 /frame   0.4% of busy  src/main.c:197  VDP_setSpriteFull(
       71 /frame   0.3% of busy  src/main.c:171  if (shipY > SCREEN_H - SHIP_H) shipY = SCREEN_H - SHIP_H;
       66 /frame   0.3% of busy  src/main.c:183  s->y = rnd() % SCREEN_H;
                 lint [div] guarded: '%' with an operand of unknown type: 453 to 1,062 cycles if the other operand is 32-bit or signed, 117 if both
       42 /frame   0.2% of busy  src/main.c:169  if (shipX > SCREEN_W - SHIP_W) shipX = SCREEN_W - SHIP_W;
       23 /frame   0.1% of busy  src/main.c:163  if (joy & BUTTON_LEFT)  shipX -= SHIP_SPEED;

== Lint findings, by measured cost ==
negligible        66 /frame (0.05% of a frame)  src/main.c:183 [div] guarded
verdicts: matters >= 1% of a frame, small >= 0.2%, negligible below, not run = no cycles on that line in this run

== Game lines over 2% of a frame with no lint finding ==
    7,548 /frame  src/main.c:187 (main), mostly own code
    4,820 /frame  src/main.c:207 (main), mostly SYS_doVBlankProcessEx.constprop.0.isra.0
    4,032 /frame  src/main.c:178 (main), mostly own code
    3,713 /frame  src/main.c:192 (main), mostly own code

library code with no game line on the stack (interrupt handlers, SGDK's VBlank work): 318 cycles per frame

main.c:
#include <genesis.h>

/* ------------------------------------------------------------------ */
/* Configuration                                                      */
/* ------------------------------------------------------------------ */

#define SCREEN_W        320
#define SCREEN_H        224

#define NUM_STARS       64
#define NUM_LAYERS      3
#define SHIP_W          16
#define SHIP_H          16
#define SHIP_SPEED      2

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
    u8  layer;      /* 0 = far, 1 = mid, 2 = near */
} Star;

static Star stars[NUM_STARS];

/* Parallax: each layer scrolls at a different speed (pixels/frame) */
static const s16 layerSpeed[NUM_LAYERS] = { 1, 2, 3 };
static const u16 layerPal[NUM_LAYERS]   = { PAL1, PAL2, PAL3 };

/* Small xorshift PRNG so the code does not depend on library RNG details */
static u16 rngState = 0xACE1;

static u16 rnd(void)
{
    rngState ^= (u16)(rngState << 7);
    rngState ^= (u16)(rngState >> 9);
    rngState ^= (u16)(rngState << 8);
    return rngState;
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */

int main(bool hardReset)
{
    u16 i;
    u16 joy;
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

    /* Distribute stars: 24 far, 24 mid, 16 near */
    for (i = 0; i < NUM_STARS; i++)
    {
        stars[i].x = rnd() % SCREEN_W;
        stars[i].y = rnd() % SCREEN_H;

        if (i < 24)
            stars[i].layer = 0;
        else if (i < 48)
            stars[i].layer = 1;
        else
            stars[i].layer = 2;
    }

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

        /* ---- Update and draw stars ---- */
        for (i = 0; i < NUM_STARS; i++)
        {
            Star *s = &stars[i];

            s->x -= layerSpeed[s->layer];

            if (s->x < 0)
            {
                s->x += SCREEN_W;
                s->y = rnd() % SCREEN_H;
            }

            /* Star sprite i+1; link to next star, last one ends the chain */
            VDP_setSpriteFull(
                i + 1,
                s->x,
                s->y,
                SPRITE_SIZE(1, 1),
                TILE_ATTR_FULL(layerPal[s->layer], 0, FALSE, FALSE, STAR_TILE),
                (i == NUM_STARS - 1) ? 0 : (i + 2));
        }

        /* ---- Draw ship (sprite 0, linked to first star) ---- */
        VDP_setSpriteFull(
            SHIP_SPRITE,
            shipX,
            shipY,
            SPRITE_SIZE(2, 2),
            TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, SHIP_TILE),
            1);

        /* ---- Push sprite table and wait for VBlank ---- */
        VDP_updateSprites(NUM_STARS + 1, DMA_QUEUE);
        SYS_doVBlankProcess();
    }

    return 0;
}
