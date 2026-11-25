Here is a Mega Drive game written with SGDK 2.09. Make it run faster on the real hardware (68000 at 7.67 MHz) without changing what the game does or how it looks. Reply with the full main.c in one ```c code block.

Below is a line profile of this code, measured in an emulator with a scripted pad: cycles per video frame by source line (one NTSC frame is 128,005 cycles), what each line called, and the findings of a performance lint on the same lines.

== Lines by measured cost (sgdkhot: mdlines + sgdklint) ==
budget 128,005 cycles per video frame (NTSC); 1,200 frames counted; busy 9,084 cycles per frame on average (7.1%)
each line = its own instructions + every library/libgcc call made from it (game helpers count on their own lines)
    3,745 /frame  41.2% of busy  src/main.c:352  SYS_doVBlankProcess();
                     3,727 in SYS_doVBlankProcessEx.constprop.0.isra.0: 1.00 calls/frame, 3,727 cycles each
    1,430 /frame  15.7% of busy  src/main.c:176  VDP_setSpriteFull(1 + i, (s16) (enemyX[i] >> 8), (s16) (enem
    1,012 /frame  11.1% of busy  src/main.c:186  VDP_updateSprites(NUM_SPRITES, DMA_QUEUE);
                       794 in DMA_queueDmaFast: 1.00 calls/frame, 794 cycles each
      565 /frame   6.2% of busy  src/main.c:238  enemyY[i] += ((s32) dy * ENEMY_SPEED) / dist;
                       458 in __divsi3: 0.79 calls/frame, 578 cycles each
                 lint [div] guarded: '/' on 32-bit / 32-bit: 617 cycles (s32 / s32); SGDK divu()/divs() is 133-139, a shift is 23. Here: 8 passes x
      559 /frame   6.1% of busy  src/main.c:237  enemyX[i] += ((s32) dx * ENEMY_SPEED) / dist;
                       457 in __divsi3: 0.79 calls/frame, 577 cycles each
                 lint [div] guarded: '/' on 32-bit / 32-bit: 617 cycles (s32 / s32); SGDK divu()/divs() is 133-139, a shift is 23. Here: 8 passes x
      318 /frame   3.5% of busy  [no game line]:0  
                       124 in no_bmp_task
                       123 in no_user_task
                        31 in no_xgm_task
      213 /frame   2.3% of busy  src/main.c:114  while (bit > n) bit >>= 2;
      182 /frame   2.0% of busy  src/main.c:170  VDP_setSpriteFull(0, (s16) (playerX >> 8), (s16) (playerY >>
      160 /frame   1.8% of busy  src/main.c:117  if (n >= res + bit)
      122 /frame   1.3% of busy  src/main.c:182  VDP_setSpriteFull(NUM_SPRITES - 1, coinX, coinY,
       90 /frame   1.0% of busy  src/main.c:230  d2 = (u32) ((s32) dx * dx + (s32) dy * dy);
       85 /frame   0.9% of busy  src/main.c:120  res = (res >> 1) + bit;
       84 /frame   0.9% of busy  src/main.c:115  while (bit)
       71 /frame   0.8% of busy  src/main.c:123  bit >>= 2;
       60 /frame   0.7% of busy  src/main.c:312  switch (state)

== Lint findings, by measured cost ==
     small       565 /frame (0.44% of a frame)  src/main.c:238 [div] guarded
     small       559 /frame (0.44% of a frame)  src/main.c:237 [div] guarded
negligible         6 /frame (0.00% of a frame)  src/main.c:139 [text] guarded
negligible         5 /frame (0.00% of a frame)  src/main.c:142 [text] guarded
negligible         5 /frame (0.00% of a frame)  src/main.c:331 [text] guarded
negligible         4 /frame (0.00% of a frame)  src/main.c:330 [text] guarded
negligible         2 /frame (0.00% of a frame)  src/main.c:138 [text] guarded
negligible         2 /frame (0.00% of a frame)  src/main.c:141 [text] guarded
negligible         1 /frame (0.00% of a frame)  src/main.c:319 [text] guarded
negligible         1 /frame (0.00% of a frame)  src/main.c:344 [text] guarded
negligible         1 /frame (0.00% of a frame)  src/main.c:343 [text] guarded
verdicts: matters >= 1% of a frame, small >= 0.2%, negligible below, not run = no cycles on that line in this run

== Game lines over 2% of a frame with no lint finding ==
    3,745 /frame  src/main.c:352 (main), mostly SYS_doVBlankProcessEx.constprop.0.isra.0

library code with no game line on the stack (interrupt handlers, SGDK's VBlank work): 318 cycles per frame

main.c:
/*
 * main.c - Top-down arena for Sega Mega Drive / Genesis (SGDK 2.09)
 *
 * - D-pad (pad 1) moves the player (diagonals are normalized).
 * - 8 enemies chase the player at a constant speed (normalized direction).
 * - Grab the coins for +10 points; touching an enemy ends the run.
 * - HUD shows SCORE and TIME (seconds survived).
 * - START begins / restarts.
 * - No resource files: all tiles and palettes are defined in code.
 */

#include <genesis.h>

/* ---------------------------------------------------------------- config */
#define NUM_ENEMIES     8
#define NUM_SPRITES     (1 + NUM_ENEMIES + 1)   /* player + enemies + coin */

/* Playfield in pixels (border tiles occupy row 2, row 27, col 0, col 39) */
#define ARENA_L         8
#define ARENA_T         24
#define ARENA_R         312
#define ARENA_B         216

#define PLAYER_SIZE     16
#define ENEMY_SIZE      8
#define COIN_SIZE       8

/* Speeds in 8.8 fixed point (pixels per frame * 256) */
#define PLAYER_SPEED    512     /* 2.00 px/frame                       */
#define PLAYER_DIAG     362     /* 2.00 * 0.7071                       */
#define ENEMY_SPEED     200     /* ~0.78 px/frame, same for all enemies */

#define HIT_DIST        9       /* center distance for player/enemy hit */
#define COIN_DIST       10      /* center distance for coin pickup      */

/* ------------------------------------------------------------ tile layout */
#define TILE_BORDER     (TILE_USER_INDEX + 0)
#define TILE_PLAYER     (TILE_USER_INDEX + 1)   /* 4 tiles (2x2 sprite) */
#define TILE_ENEMY      (TILE_USER_INDEX + 5)
#define TILE_COIN       (TILE_USER_INDEX + 6)
#define NUM_GAME_TILES  7

/* Each u32 is one 8-pixel row, one nibble (palette index) per pixel. */
static const u32 gameTiles[NUM_GAME_TILES * 8] =
{
    /* Border brick (PAL0 idx 3 / 4) */
    0x33333334, 0x33333334, 0x33333334, 0x33333334,
    0x33333334, 0x33333334, 0x33333334, 0x44444444,

    /* Player 16x16, sprite tile order is column-major: TL, BL, TR, BR */
    /* TL */
    0x00222222, 0x02211111, 0x22111111, 0x21111111,
    0x21111111, 0x21111111, 0x21111111, 0x21111111,
    /* BL */
    0x21111111, 0x21111111, 0x21111111, 0x21111111,
    0x21111111, 0x22111111, 0x02211111, 0x00222222,
    /* TR */
    0x22222200, 0x11111220, 0x11111122, 0x11111112,
    0x11111112, 0x11111112, 0x11111112, 0x11111112,
    /* BR */
    0x11111112, 0x11111112, 0x11111112, 0x11111112,
    0x11111112, 0x11111122, 0x11111220, 0x22222200,

    /* Enemy 8x8 */
    0x00111100, 0x01111110, 0x11211211, 0x11111111,
    0x11222211, 0x11111111, 0x01111110, 0x00111100,

    /* Coin 8x8 */
    0x00111100, 0x01122110, 0x11122111, 0x11122111,
    0x11122111, 0x11122111, 0x01122110, 0x00111100
};

/* Colors are 0x0BGR (each channel 0..E, even values) */
static const u16 palText[16] =
{
    0x0000, 0x0EEE, 0x0AAA, 0x0888, 0x0444, 0x0EEE, 0x0EEE, 0x0EEE,
    0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE
};
static const u16 palPlayer[16] = { 0x0000, 0x02E2, 0x08E8 };  /* greens  */
static const u16 palEnemy[16]  = { 0x0000, 0x022E, 0x0006 };  /* reds    */
static const u16 palCoin[16]   = { 0x0000, 0x00EE, 0x008E };  /* yellows */

/* ------------------------------------------------------------------ state */
typedef enum { STATE_TITLE, STATE_PLAY, STATE_OVER } GameState;

static GameState state;

/* Positions are top-left of the sprite, 8.8 fixed point */
static s32 playerX, playerY;
static s32 enemyX[NUM_ENEMIES], enemyY[NUM_ENEMIES];
static s16 coinX, coinY;            /* integer pixels */

static u32 score;
static u32 seconds;
static u16 frameInSecond;
static u16 framesPerSecond;
static u16 titleFrames;

/* Enemy spawn points (pixels, top-left) */
static const s16 enemySpawn[NUM_ENEMIES][2] =
{
    { 12,  28 }, { 300,  28 }, { 12, 204 }, { 300, 204 },
    { 156, 28 }, { 156, 204 }, { 12, 116 }, { 300, 116 }
};

/* ---------------------------------------------------------------- helpers */

/* Integer square root (bit-by-bit, no division) */
static u16 isqrt32(u32 n)
{
    u32 res = 0;
    u32 bit = 1UL << 30;

    while (bit > n) bit >>= 2;
    while (bit)
    {
        if (n >= res + bit)
        {
            n -= res + bit;
            res = (res >> 1) + bit;
        }
        else res >>= 1;
        bit >>= 2;
    }
    return (u16) res;
}

static void spawnCoin(void)
{
    coinX = 16 + (s16) (random() % 280);    /* 16..295 */
    coinY = 32 + (s16) (random() % 168);    /* 32..199 */
}

static void drawHud(void)
{
    char buf[12];

    uintToStr(score, buf, 5);
    VDP_drawText(buf, 7, 0);

    uintToStr(seconds, buf, 4);
    VDP_drawText(buf, 31, 0);
}

static void resetGame(void)
{
    u16 i;

    playerX = 152L << 8;
    playerY = 112L << 8;

    for (i = 0; i < NUM_ENEMIES; i++)
    {
        enemyX[i] = ((s32) enemySpawn[i][0]) << 8;
        enemyY[i] = ((s32) enemySpawn[i][1]) << 8;
    }

    spawnCoin();

    score = 0;
    seconds = 0;
    frameInSecond = 0;
    drawHud();
}

static void drawSprites(void)
{
    u16 i;

    VDP_setSpriteFull(0, (s16) (playerX >> 8), (s16) (playerY >> 8),
                      SPRITE_SIZE(2, 2),
                      TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, TILE_PLAYER), 1);

    for (i = 0; i < NUM_ENEMIES; i++)
    {
        VDP_setSpriteFull(1 + i, (s16) (enemyX[i] >> 8), (s16) (enemyY[i] >> 8),
                          SPRITE_SIZE(1, 1),
                          TILE_ATTR_FULL(PAL2, FALSE, FALSE, FALSE, TILE_ENEMY),
                          2 + i);
    }

    VDP_setSpriteFull(NUM_SPRITES - 1, coinX, coinY,
                      SPRITE_SIZE(1, 1),
                      TILE_ATTR_FULL(PAL3, FALSE, FALSE, FALSE, TILE_COIN), 0);

    VDP_updateSprites(NUM_SPRITES, DMA_QUEUE);
}

static void updatePlayer(void)
{
    u16 j = JOY_readJoypad(JOY_1);
    s16 dx = 0, dy = 0, step;

    if (j & BUTTON_LEFT)       dx = -1;
    else if (j & BUTTON_RIGHT) dx = 1;
    if (j & BUTTON_UP)         dy = -1;
    else if (j & BUTTON_DOWN)  dy = 1;

    step = (dx && dy) ? PLAYER_DIAG : PLAYER_SPEED;

    playerX += (s32) dx * step;
    playerY += (s32) dy * step;

    if (playerX < ((s32) ARENA_L << 8))                 playerX = (s32) ARENA_L << 8;
    if (playerX > ((s32) (ARENA_R - PLAYER_SIZE) << 8)) playerX = (s32) (ARENA_R - PLAYER_SIZE) << 8;
    if (playerY < ((s32) ARENA_T << 8))                 playerY = (s32) ARENA_T << 8;
    if (playerY > ((s32) (ARENA_B - PLAYER_SIZE) << 8)) playerY = (s32) (ARENA_B - PLAYER_SIZE) << 8;
}

/* Returns TRUE if an enemy touched the player */
static bool updateEnemies(void)
{
    u16 i;
    bool hit = FALSE;
    s16 pcx = (s16) (playerX >> 8) + PLAYER_SIZE / 2;
    s16 pcy = (s16) (playerY >> 8) + PLAYER_SIZE / 2;

    for (i = 0; i < NUM_ENEMIES; i++)
    {
        s16 ecx = (s16) (enemyX[i] >> 8) + ENEMY_SIZE / 2;
        s16 ecy = (s16) (enemyY[i] >> 8) + ENEMY_SIZE / 2;
        s16 dx = pcx - ecx;
        s16 dy = pcy - ecy;
        u32 d2;
        s32 dist;

        if (dx > -HIT_DIST && dx < HIT_DIST && dy > -HIT_DIST && dy < HIT_DIST)
            hit = TRUE;

        d2 = (u32) ((s32) dx * dx + (s32) dy * dy);
        if (d2 == 0) continue;

        dist = (s32) isqrt32(d2);
        if (dist == 0) continue;

        /* velocity = unit direction * ENEMY_SPEED  (constant speed) */
        enemyX[i] += ((s32) dx * ENEMY_SPEED) / dist;
        enemyY[i] += ((s32) dy * ENEMY_SPEED) / dist;
    }
    return hit;
}

static void checkCoin(void)
{
    s16 dx = (s16) (playerX >> 8) + PLAYER_SIZE / 2 - (coinX + COIN_SIZE / 2);
    s16 dy = (s16) (playerY >> 8) + PLAYER_SIZE / 2 - (coinY + COIN_SIZE / 2);

    if (dx > -COIN_DIST && dx < COIN_DIST && dy > -COIN_DIST && dy < COIN_DIST)
    {
        score += 10;
        spawnCoin();
        drawHud();
    }
}

static void updateTimer(void)
{
    frameInSecond++;
    if (frameInSecond >= framesPerSecond)
    {
        frameInSecond = 0;
        seconds++;
        drawHud();
    }
}

/* ------------------------------------------------------------------- main */
int main(bool hardReset)
{
    (void) hardReset;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    /* Palettes */
    PAL_setPalette(PAL0, palText, CPU);
    PAL_setPalette(PAL1, palPlayer, CPU);
    PAL_setPalette(PAL2, palEnemy, CPU);
    PAL_setPalette(PAL3, palCoin, CPU);

    /* Tiles defined in code */
    VDP_loadTileData(gameTiles, TILE_BORDER, NUM_GAME_TILES, CPU);

    /* Arena border on BG_B (text goes on BG_A) */
    VDP_clearPlane(BG_B, TRUE);
    {
        u16 attr = TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_BORDER);
        VDP_fillTileMapRect(BG_B, attr, 0, 2, 40, 1);    /* top    */
        VDP_fillTileMapRect(BG_B, attr, 0, 27, 40, 1);   /* bottom */
        VDP_fillTileMapRect(BG_B, attr, 0, 2, 1, 26);    /* left   */
        VDP_fillTileMapRect(BG_B, attr, 39, 2, 1, 26);   /* right  */
    }

    /* HUD labels */
    VDP_drawText("SCORE", 1, 0);
    VDP_drawText("TIME", 26, 0);

    framesPerSecond = IS_PAL_SYSTEM ? 50 : 60;

    VDP_clearSprites();
    resetGame();
    drawSprites();

    state = STATE_TITLE;
    titleFrames = 0;
    VDP_drawText("PRESS START", 14, 13);

    while (TRUE)
    {
        u16 pad = JOY_readJoypad(JOY_1);

        switch (state)
        {
            case STATE_TITLE:
                titleFrames++;
                if (pad & BUTTON_START)
                {
                    setRandomSeed(titleFrames | 1);
                    VDP_clearText(14, 13, 11);
                    resetGame();
                    state = STATE_PLAY;
                }
                break;

            case STATE_PLAY:
                updatePlayer();
                if (updateEnemies())
                {
                    state = STATE_OVER;
                    VDP_drawText("GAME OVER", 15, 12);
                    VDP_drawText("PRESS START", 14, 14);
                }
                else
                {
                    checkCoin();
                    updateTimer();
                }
                break;

            case STATE_OVER:
                if (pad & BUTTON_START)
                {
                    VDP_clearText(15, 12, 9);
                    VDP_clearText(14, 14, 11);
                    resetGame();
                    state = STATE_PLAY;
                }
                break;
        }

        drawSprites();
        SYS_doVBlankProcess();
    }

    return 0;
}
