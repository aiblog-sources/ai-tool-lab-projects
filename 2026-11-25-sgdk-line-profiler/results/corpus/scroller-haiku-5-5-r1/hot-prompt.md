Here is a Mega Drive game written with SGDK 2.09. Make it run faster on the real hardware (68000 at 7.67 MHz) without changing what the game does or how it looks. Reply with the full main.c in one ```c code block.

Below is a line profile of this code, measured in an emulator with a scripted pad: cycles per video frame by source line (one NTSC frame is 128,005 cycles), what each line called, and the findings of a performance lint on the same lines.

== Lines by measured cost (sgdkhot: mdlines + sgdklint) ==
budget 128,005 cycles per video frame (NTSC); 1,200 frames counted; busy 6,351 cycles per frame on average (5.0%)
each line = its own instructions + every library/libgcc call made from it (game helpers count on their own lines)
    3,425 /frame  53.9% of busy  src/main.c:202  SYS_doVBlankProcess();
                     3,395 in SYS_doVBlankProcessEx.constprop.0.isra.0: 1.00 calls/frame, 3,395 cycles each
      933 /frame  14.7% of busy  src/main.c:138  VDP_setTileMapXY(BG_B,
                 lint [tile-loop] hot: VDP_setTileMapXY in a loop: 324 cycles per tile; VDP_fillTileMapRect is 18 per tile, VDP_setTileMapDataRect 37
      933 /frame  14.7% of busy  src/main.c:194  VDP_updateSprites(1, DMA);
                       772 in DMA_doDmaFast: 1.00 calls/frame, 772 cycles each
      318 /frame   5.0% of busy  [no game line]:0  
                       125 in no_bmp_task
                       123 in no_user_task
                        31 in no_xgm_task
      175 /frame   2.8% of busy  src/main.c:200  VDP_setHorizontalScroll(BG_B, -(s16)(camX & (PLANE_W_PIXELS 
      116 /frame   1.8% of busy  src/main.c:125  for (row = 0; row < SCREEN_TILE_H; row++)
       76 /frame   1.2% of busy  src/main.c:149  while ((genCol << 3) < camX + SCREEN_W + 8)
       76 /frame   1.2% of busy  src/main.c:192  VDP_setSprite(0, px, py, SPRITE_SIZE(2, 2),
       62 /frame   1.0% of busy  src/main.c:131  if (below <= 0)
       36 /frame   0.6% of busy  src/main.c:188  if (px > 304) px = 304;   /* 320 - 16 */
       35 /frame   0.5% of busy  src/main.c:190  if (py > 208) py = 208;   /* 224 - 16 */
       34 /frame   0.5% of busy  src/main.c:133  else if (below >= 9)
       21 /frame   0.3% of busy  src/main.c:182  if (joy & BUTTON_LEFT)  px -= 2;
       21 /frame   0.3% of busy  src/main.c:183  if (joy & BUTTON_RIGHT) px += 2;
       20 /frame   0.3% of busy  src/main.c:185  if (joy & BUTTON_DOWN)  py += 2;

== Lint findings, by measured cost ==
     small       933 /frame (0.73% of a frame)  src/main.c:138 [tile-loop] hot
verdicts: matters >= 1% of a frame, small >= 0.2%, negligible below, not run = no cycles on that line in this run

== Game lines over 2% of a frame with no lint finding ==
    3,425 /frame  src/main.c:202 (main), mostly SYS_doVBlankProcessEx.constprop.0.isra.0

library code with no game line on the stack (interrupt handlers, SGDK's VBlank work): 318 cycles per frame

main.c:
#include <genesis.h>

/* ---------------- Screen / plane constants ---------------- */
#define SCREEN_W          320
#define SCREEN_H          224
#define SCREEN_TILE_H     (SCREEN_H / 8)   /* 28 tile rows */
#define PLANE_W_TILES     64               /* plane B: 64 x 32 tiles = 512 x 256 px */
#define PLANE_W_PIXELS    (PLANE_W_TILES * 8)

#define SCROLL_SPEED      1                /* background scroll, px per frame */

/* VRAM tile indices */
#define BG_TILE_BASE      1                /* hill tiles 0..9 -> VRAM 1..10 */
#define PLAYER_TILE       20               /* 2x2 player tiles -> VRAM 20..23 */

/* ---------------- Background tiles ----------------
 * 4bpp 8x8 tiles: 8 x u32, one u32 per pixel row, leftmost pixel in the
 * high nibble. Colour indices: 0 = sky, 1 = grass, 2 = ground body.
 *
 * Tile k (0..9):
 *   k = 0      : all sky
 *   k = 1..8   : bottom k pixel rows are ground; the top row of that
 *                ground part is grass, the rest is body
 *   k = 9      : solid ground
 */
#define C_SKY     0x00000000UL
#define C_GRASS   0x11111111UL
#define C_BODY    0x22222222UL

const u32 hillTiles[10 * 8] =
{
    /* 0 */ C_SKY, C_SKY, C_SKY, C_SKY, C_SKY, C_SKY, C_SKY, C_SKY,
    /* 1 */ C_SKY, C_SKY, C_SKY, C_SKY, C_SKY, C_SKY, C_SKY, C_GRASS,
    /* 2 */ C_SKY, C_SKY, C_SKY, C_SKY, C_SKY, C_SKY, C_GRASS, C_BODY,
    /* 3 */ C_SKY, C_SKY, C_SKY, C_SKY, C_SKY, C_GRASS, C_BODY, C_BODY,
    /* 4 */ C_SKY, C_SKY, C_SKY, C_SKY, C_GRASS, C_BODY, C_BODY, C_BODY,
    /* 5 */ C_SKY, C_SKY, C_SKY, C_GRASS, C_BODY, C_BODY, C_BODY, C_BODY,
    /* 6 */ C_SKY, C_SKY, C_GRASS, C_BODY, C_BODY, C_BODY, C_BODY, C_BODY,
    /* 7 */ C_SKY, C_GRASS, C_BODY, C_BODY, C_BODY, C_BODY, C_BODY, C_BODY,
    /* 8 */ C_GRASS, C_BODY, C_BODY, C_BODY, C_BODY, C_BODY, C_BODY, C_BODY,
    /* 9 */ C_BODY, C_BODY, C_BODY, C_BODY, C_BODY, C_BODY, C_BODY, C_BODY
};

/* ---------------- Player sprite tiles (16x16 = 2x2 tiles) ----------------
 * Hardware sprite tile order is column-major: TL, BL, TR, BR.
 * Colour indices: 0 = transparent, 1 = body, 2 = highlight, 3 = outline.
 */
#define P_OUT      0x33333333UL
#define P_HL_L     0x32222222UL
#define P_BODY_L   0x31111111UL
#define P_HL_R     0x22222223UL
#define P_BODY_R   0x11111113UL

const u32 playerTiles[4 * 8] =
{
    /* TL */ P_OUT,   P_HL_L,   P_BODY_L, P_BODY_L, P_BODY_L, P_BODY_L, P_BODY_L, P_BODY_L,
    /* BL */ P_BODY_L, P_BODY_L, P_BODY_L, P_BODY_L, P_BODY_L, P_BODY_L, P_BODY_L, P_OUT,
    /* TR */ P_OUT,   P_HL_R,   P_BODY_R, P_BODY_R, P_BODY_R, P_BODY_R, P_BODY_R, P_BODY_R,
    /* BR */ P_BODY_R, P_BODY_R, P_BODY_R, P_BODY_R, P_BODY_R, P_BODY_R, P_BODY_R, P_OUT
};

/* ---------------- Palettes (0x0BGR, 3 bits per channel) ---------------- */
const u16 bgPalette[16] =
{
    0x0EA4,   /* 0: sky blue   */
    0x04E6,   /* 1: grass      */
    0x0282,   /* 2: ground     */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

const u16 spritePalette[16] =
{
    0x0000,   /* 0: transparent */
    0x00EE,   /* 1: yellow body */
    0x0EEE,   /* 2: white highlight */
    0x0004,   /* 3: dark red outline */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* ---------------- Procedural hill generator ---------------- */
static u16 rngState = 0xACE1;

static u16 rnd(void)
{
    rngState = (u16)(rngState * 25173u + 13849u);
    return (u16)(rngState >> 8);
}

static s16 surfY    = 160;   /* current surface height (px from top of screen) */
static s16 targetY  = 160;   /* surface height we are heading towards */
static u16 holdCols = 0;     /* columns to stay flat before moving again */
static u32 genCol   = 0;     /* next world column (8px) to generate */

static void stepSurface(void)
{
    s16 diff;

    if (holdCols)
    {
        holdCols--;
        return;
    }

    diff = targetY - surfY;
    if (diff == 0)
    {
        /* reached target: choose a new one, maybe hold flat for a while */
        targetY  = (s16)(96 + (rnd() % 96));   /* 96..191 */
        holdCols = rnd() % 24;
        return;
    }

    if (diff > 0)
        surfY += (diff >= 2) ? 2 : 1;
    else
        surfY -= (diff <= -2) ? 2 : 1;
}

static void generateColumn(u32 col)
{
    u16 mapX = (u16)(col & (PLANE_W_TILES - 1));
    s16 surf = surfY;
    u16 row;

    for (row = 0; row < SCREEN_TILE_H; row++)
    {
        /* number of ground pixels inside this tile (<=0 sky, >=9 solid) */
        s16 below = (s16)(row * 8 + 8) - surf;
        u16 idx;

        if (below <= 0)
            idx = 0;
        else if (below >= 9)
            idx = 9;
        else
            idx = (u16)below;

        VDP_setTileMapXY(BG_B,
                         TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, BG_TILE_BASE + idx),
                         mapX, row);
    }

    stepSurface();
}

/* Make sure every column up to the right edge of the view (plus a margin) exists */
static void fillAhead(u32 camX)
{
    while ((genCol << 3) < camX + SCREEN_W + 8)
    {
        generateColumn(genCol);
        genCol++;
    }
}

/* ---------------- Main ---------------- */
int main(void)
{
    u32 camX = 0;
    s16 px = 152;
    s16 py = 104;
    u16 joy;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();
    VDP_setPlaneSize(64, 32, TRUE);
    VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);

    VDP_loadTileData(hillTiles,   BG_TILE_BASE, 10, CPU);
    VDP_loadTileData(playerTiles, PLAYER_TILE,   4, CPU);

    PAL_setPalette(PAL0, bgPalette,     CPU);
    PAL_setPalette(PAL1, spritePalette, CPU);

    fillAhead(camX);

    while (TRUE)
    {
        /* player movement */
        joy = JOY_readJoypad(JOY_1);

        if (joy & BUTTON_LEFT)  px -= 2;
        if (joy & BUTTON_RIGHT) px += 2;
        if (joy & BUTTON_UP)    py -= 2;
        if (joy & BUTTON_DOWN)  py += 2;

        if (px < 0)   px = 0;
        if (px > 304) px = 304;   /* 320 - 16 */
        if (py < 0)   py = 0;
        if (py > 208) py = 208;   /* 224 - 16 */

        VDP_setSprite(0, px, py, SPRITE_SIZE(2, 2),
                      TILE_ATTR_FULL(PAL1, 0, FALSE, FALSE, PLAYER_TILE));
        VDP_updateSprites(1, DMA);

        /* scroll: camera moves right, so the hills move left.
         * Plane B wraps every 512 px, so only the low bits matter. */
        camX += SCROLL_SPEED;
        fillAhead(camX);
        VDP_setHorizontalScroll(BG_B, -(s16)(camX & (PLANE_W_PIXELS - 1)));

        SYS_doVBlankProcess();
    }

    return 0;
}
