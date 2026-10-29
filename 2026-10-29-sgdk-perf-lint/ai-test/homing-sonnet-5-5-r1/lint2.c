/*
 * Top-down arena - SGDK 2.09
 * Single file, no resources (no rescomp). All tiles are const u32 arrays
 * loaded with VDP_loadTileData.
 *
 * - D-pad (pad 1) moves the player (green).
 * - 8 enemies (red) chase the player at constant speed (normalized direction).
 * - Yellow gems give +100. Surviving gives +10 per second.
 * - Touching an enemy ends the game. Press START to restart.
 * - HUD shows score and timer in seconds.
 */

#include <genesis.h>

/* ------------------------------------------------------------------ */
/* Tiles (8x8, 4bpp: one u32 per row, one nibble per pixel)            */
/* ------------------------------------------------------------------ */

#define TILE_COUNT   5
#define IDX_FLOOR    (TILE_USER_INDEX + 0)
#define IDX_WALL     (TILE_USER_INDEX + 1)
#define IDX_PLAYER   (TILE_USER_INDEX + 2)
#define IDX_ENEMY    (TILE_USER_INDEX + 3)
#define IDX_GEM      (TILE_USER_INDEX + 4)

static const u32 gameTiles[TILE_COUNT * 8] =
{
    /* 0: floor (PAL2) */
    0x11111111,
    0x11111111,
    0x11111111,
    0x11111111,
    0x11121111,
    0x11111111,
    0x11111111,
    0x11111111,

    /* 1: wall (PAL2) */
    0x33333334,
    0x33333334,
    0x33333334,
    0x44444444,
    0x33433333,
    0x33433333,
    0x33433333,
    0x44444444,

    /* 2: player (PAL1) */
    0x00111100,
    0x01222210,
    0x12233221,
    0x12233221,
    0x12222221,
    0x12222221,
    0x01222210,
    0x00111100,

    /* 3: enemy (PAL3) */
    0x00111100,
    0x01222210,
    0x12322321,
    0x12222221,
    0x12222221,
    0x12122121,
    0x01211210,
    0x00111100,

    /* 4: gem (PAL1, colors 4/5) */
    0x00055000,
    0x00544500,
    0x05444450,
    0x54444445,
    0x54444445,
    0x05444450,
    0x00544500,
    0x00055000
};

/* ------------------------------------------------------------------ */
/* Palettes                                                            */
/* ------------------------------------------------------------------ */

static const u16 palPlayer[6] =
{
    0,                                  /* 0: transparent */
    RGB24_TO_VDPCOLOR(0x004000),        /* 1: dark green outline */
    RGB24_TO_VDPCOLOR(0x20C020),        /* 2: green */
    RGB24_TO_VDPCOLOR(0xC0FFC0),        /* 3: highlight */
    RGB24_TO_VDPCOLOR(0xFFE020),        /* 4: yellow */
    RGB24_TO_VDPCOLOR(0xA07000)         /* 5: dark yellow */
};

static const u16 palBackground[5] =
{
    0,
    RGB24_TO_VDPCOLOR(0x101828),        /* 1: floor */
    RGB24_TO_VDPCOLOR(0x203048),        /* 2: floor dot */
    RGB24_TO_VDPCOLOR(0x806040),        /* 3: wall */
    RGB24_TO_VDPCOLOR(0x402818)         /* 4: wall mortar */
};

static const u16 palEnemy[4] =
{
    0,
    RGB24_TO_VDPCOLOR(0x500000),        /* 1: dark red outline */
    RGB24_TO_VDPCOLOR(0xE02020),        /* 2: red */
    RGB24_TO_VDPCOLOR(0xFFFFFF)         /* 3: eyes */
};

/* ------------------------------------------------------------------ */
/* Game constants                                                      */
/* ------------------------------------------------------------------ */

#define NUM_ENEMIES     8
#define SPR_PLAYER      0
#define SPR_ENEMY0      1
#define SPR_GEM         (SPR_ENEMY0 + NUM_ENEMIES)
#define NUM_SPRITES     (SPR_GEM + 1)

/* Playable area in pixels (sprite top-left, 8x8 sprites) */
#define MIN_X           8
#define MAX_X           304
#define MIN_Y           24
#define MAX_Y           208

/* Speeds in 8.8 fixed point, pixels per frame */
#define PLAYER_SPEED    384             /* 1.5 px/frame */
#define PLAYER_DIAG     272             /* 1.5 * 0.707  */
#define ENEMY_SPEED     192             /* 0.75 px/frame, constant */

#define HIT_DIST        6               /* enemy collision distance */
#define GEM_DIST        8

/* ------------------------------------------------------------------ */
/* Game state                                                          */
/* ------------------------------------------------------------------ */

static s32  px, py;                     /* player, 8.8 fixed */
static s32  ex[NUM_ENEMIES];            /* enemies, 8.8 fixed */
static s32  ey[NUM_ENEMIES];
static s16  gemX, gemY;                 /* gem, pixels */

static u32  score;
static u16  seconds;
static u16  frames;
static u16  fps;
static bool gameOver;
static bool scoreDirty;
static bool timeDirty;

/* Enemy start positions: corners and edge midpoints */
static const s16 spawnX[NUM_ENEMIES] = { MIN_X, 156, MAX_X, MIN_X, MAX_X, MIN_X, 156, MAX_X };
static const s16 spawnY[NUM_ENEMIES] = { MIN_Y, MIN_Y, MIN_Y, 116, 116, MAX_Y, MAX_Y, MAX_Y };

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

/* Integer square root (no floats on 68000) */
static u32 isqrt32(u32 n)
{
    u32 res = 0;
    u32 bit = 1UL << 30;

    while (bit > n) bit >>= 2;

    while (bit != 0)
    {
        if (n >= res + bit)
        {
            n -= res + bit;
            res = (res >> 1) + bit;
        }
        else
        {
            res >>= 1;
        }
        bit >>= 2;
    }
    return res;
}

static s32 clampS32(s32 v, s32 lo, s32 hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static s16 absS16(s16 v)
{
    return (v < 0) ? -v : v;
}

static void placeGem(void)
{
    gemX = 16 + (random() % 280);       /* 16..295  */
    gemY = 32 + (random() % 168);       /* 32..199  */
}

/* Each value is redrawn only when it has changed */
static void drawHud(void)
{
    char buf[12];

    if (scoreDirty)
    {
        uintToStr(score, buf, 6);
        VDP_drawText(buf, 7, 0);
        scoreDirty = FALSE;
    }

    if (timeDirty)
    {
        uintToStr(seconds, buf, 3);
        VDP_drawText(buf, 26, 0);
        timeDirty = FALSE;
    }
}

static void resetGame(void)
{
    u16 i;

    px = (s32)160 << 8;
    py = (s32)120 << 8;

    for (i = 0; i < NUM_ENEMIES; i++)
    {
        ex[i] = (s32)spawnX[i] << 8;
        ey[i] = (s32)spawnY[i] << 8;
    }

    placeGem();

    score = 0;
    seconds = 0;
    frames = 0;
    gameOver = FALSE;
    scoreDirty = TRUE;
    timeDirty = TRUE;

    VDP_clearTextArea(8, 12, 24, 3);
}

/*
 * Writes the sprite table. The hardware sprite coordinates have a
 * 128 pixel offset, which is added here.
 */
static void drawSprites(void)
{
    u16 i;

    VDP_setSpriteFull(SPR_PLAYER, (px >> 8) + 128, (py >> 8) + 128,
                      SPRITE_SIZE(1, 1),
                      TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, IDX_PLAYER),
                      SPR_ENEMY0);

    for (i = 0; i < NUM_ENEMIES; i++)
    {
        VDP_setSpriteFull(SPR_ENEMY0 + i, (ex[i] >> 8) + 128, (ey[i] >> 8) + 128,
                          SPRITE_SIZE(1, 1),
                          TILE_ATTR_FULL(PAL3, TRUE, FALSE, FALSE, IDX_ENEMY),
                          SPR_ENEMY0 + i + 1);
    }

    /* Last sprite: link = 0 ends the list */
    VDP_setSpriteFull(SPR_GEM, gemX + 128, gemY + 128,
                      SPRITE_SIZE(1, 1),
                      TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, IDX_GEM),
                      0);

    VDP_updateSprites(NUM_SPRITES, DMA_QUEUE);
}

static void updatePlayer(u16 joy)
{
    s16 dx = 0, dy = 0;
    s32 spd;

    if (joy & BUTTON_LEFT)  dx = -1;
    if (joy & BUTTON_RIGHT) dx = 1;
    if (joy & BUTTON_UP)    dy = -1;
    if (joy & BUTTON_DOWN)  dy = 1;

    spd = (dx != 0 && dy != 0) ? PLAYER_DIAG : PLAYER_SPEED;

    px = clampS32(px + dx * spd, (s32)MIN_X << 8, (s32)MAX_X << 8);
    py = clampS32(py + dy * spd, (s32)MIN_Y << 8, (s32)MAX_Y << 8);
}

/* Each enemy moves ENEMY_SPEED toward the player along the normalized direction */
static void updateEnemies(void)
{
    u16 i;

    for (i = 0; i < NUM_ENEMIES; i++)
    {
        /* Difference in 1/16 pixel units: max ~5120, squared fits in s32 */
        s32 dx = (px - ex[i]) >> 4;
        s32 dy = (py - ey[i]) >> 4;
        s32 dist = (s32)isqrt32((u32)(dx * dx + dy * dy));

        if (dist > 0)
        {
            /* (dx / dist) is the unit vector; multiply by speed.
             * |quotient| <= ENEMY_SPEED and dist < 32768, so the 32/16
             * hardware divide cannot overflow. */
            ex[i] += divs(dx * ENEMY_SPEED, (s16)dist);
            ey[i] += divs(dy * ENEMY_SPEED, (s16)dist);
        }
    }
}

static bool checkEnemyHit(void)
{
    u16 i;
    s16 pxp = px >> 8;
    s16 pyp = py >> 8;

    for (i = 0; i < NUM_ENEMIES; i++)
    {
        s16 dx = (ex[i] >> 8) - pxp;
        s16 dy = (ey[i] >> 8) - pyp;
        if (absS16(dx) < HIT_DIST && absS16(dy) < HIT_DIST) return TRUE;
    }
    return FALSE;
}

static void checkGem(void)
{
    s16 dx = gemX - (px >> 8);
    s16 dy = gemY - (py >> 8);

    if (absS16(dx) < GEM_DIST && absS16(dy) < GEM_DIST)
    {
        score += 100;
        placeGem();
        scoreDirty = TRUE;
    }
}

static void updateTimer(void)
{
    frames++;
    if (frames >= fps)
    {
        frames = 0;
        seconds++;
        score += 10;
        scoreDirty = TRUE;
        timeDirty = TRUE;
    }
}

/* ------------------------------------------------------------------ */
/* Setup                                                               */
/* ------------------------------------------------------------------ */

static void initGraphics(void)
{
    u16 wallAttr  = TILE_ATTR_FULL(PAL2, FALSE, FALSE, FALSE, IDX_WALL);
    u16 floorAttr = TILE_ATTR_FULL(PAL2, FALSE, FALSE, FALSE, IDX_FLOOR);

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    /* Palettes */
    PAL_setColors(PAL1 * 16, palPlayer, 6, CPU);
    PAL_setColors(PAL2 * 16, palBackground, 5, CPU);
    PAL_setColors(PAL3 * 16, palEnemy, 4, CPU);
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x000000));
    PAL_setColor(15, RGB24_TO_VDPCOLOR(0xFFFFFF));      /* text color */

    /* Tiles */
    VDP_loadTileData(gameTiles, TILE_USER_INDEX, TILE_COUNT, CPU);

    /* Arena on BG_B: floor inside, walls around (rows 2..27, cols 0..39) */
    VDP_fillTileMapRect(BG_B, floorAttr, 1, 3, 38, 24);
    VDP_fillTileMapRect(BG_B, wallAttr, 0, 2, 40, 1);
    VDP_fillTileMapRect(BG_B, wallAttr, 0, 27, 40, 1);
    VDP_fillTileMapRect(BG_B, wallAttr, 0, 2, 1, 26);
    VDP_fillTileMapRect(BG_B, wallAttr, 39, 2, 1, 26);

    /* HUD labels on BG_A (values are drawn by drawHud) */
    VDP_drawText("SCORE", 1, 0);
    VDP_drawText("TIME", 20, 0);
    VDP_drawText("S", 30, 0);

    VDP_resetSprites();
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int main(bool hardReset)
{
    u16 joy;
    u16 prevJoy = 0;
    u16 pressed;

    (void)hardReset;

    fps = IS_PAL_SYSTEM ? 50 : 60;

    initGraphics();
    setRandomSeed((u16)(getTick() | 1));
    resetGame();

    while (TRUE)
    {
        joy = JOY_readJoypad(JOY_1);
        pressed = joy & ~prevJoy;
        prevJoy = joy;

        if (!gameOver)
        {
            updatePlayer(joy);
            updateEnemies();
            checkGem();
            updateTimer();

            if (checkEnemyHit())
            {
                gameOver = TRUE;
                VDP_drawText("GAME  OVER", 15, 12);
                VDP_drawText("PRESS START", 14, 14);
            }
        }
        else if (pressed & BUTTON_START)
        {
            resetGame();
        }

        if (scoreDirty || timeDirty) drawHud();

        drawSprites();
        SYS_doVBlankProcess();
    }

    return 0;
}
