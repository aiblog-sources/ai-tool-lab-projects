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

/*
 * Native 68000 DIVS.W: 32-bit dividend / 16-bit divisor -> 16-bit quotient
 * (truncating toward zero, same as C). Only used where the quotient is
 * guaranteed to fit in 16 bits (|dx*200| / floor(dist) <= 200), which avoids
 * the very slow libgcc __divsi3 call.
 */
static inline s16 divs32by16(s32 n, s16 d)
{
    __asm__ ("divs.w %1,%0" : "+d" (n) : "dm" (d) : "cc");
    return (s16) n;
}

/* Integer square root (bit-by-bit, no division).
 * Inputs here are < 2^17, so start at 4^8 instead of 4^15. */
static u16 isqrt32(u32 n)
{
    u32 res = 0;
    u32 bit = 1UL << 16;

    while (bit > n) bit >>= 2;
    while (bit)
    {
        u32 t = res + bit;
        if (n >= t)
        {
            n -= t;
            res = (res >> 1) + bit;
        }
        else res >>= 1;
        bit >>= 2;
    }
    return (u16) res;
}

static void spawnCoin(void)
{
    coinX = 16 + (s16) ((u16) random() % (u16) 280);    /* 16..295 */
    coinY = 32 + (s16) ((u16) random() % (u16) 168);    /* 32..199 */
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

static void updatePlayer(u16 j)
{
    s16 dx = 0, dy = 0, step;

    if (j & BUTTON_LEFT)       dx = -1;
    else if (j & BUTTON_RIGHT) dx = 1;
    if (j & BUTTON_UP)         dy = -1;
    else if (j & BUTTON_DOWN)  dy = 1;

    step = (dx && dy) ? PLAYER_DIAG : PLAYER_SPEED;

    if (dx) playerX += (dx < 0) ? -(s32) step : (s32) step;
    if (dy) playerY += (dy < 0) ? -(s32) step : (s32) step;

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
        s16 dist;

        if (dx > -HIT_DIST && dx < HIT_DIST && dy > -HIT_DIST && dy < HIT_DIST)
            hit = TRUE;

        d2 = (u32) ((s32) dx * dx + (s32) dy * dy);
        if (d2 == 0) continue;

        /* d2 >= 1 guarantees dist >= 1 */
        dist = (s16) isqrt32(d2);

        /* velocity = unit direction * ENEMY_SPEED  (constant speed) */
        enemyX[i] += divs32by16((s32) dx * ENEMY_SPEED, dist);
        enemyY[i] += divs32by16((s32) dy * ENEMY_SPEED, dist);
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
    bool dirty;

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

        /* Sprites only need re-uploading when something actually moved */
        dirty = FALSE;

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
                    dirty = TRUE;
                }
                break;

            case STATE_PLAY:
                dirty = TRUE;
                updatePlayer(pad);
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
                    dirty = TRUE;
                }
                break;
        }

        if (dirty) drawSprites();
        SYS_doVBlankProcess();
    }

    return 0;
}
