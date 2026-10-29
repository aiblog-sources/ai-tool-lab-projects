/*
 * main.c - Homing Arena (SGDK 2.09, single file, no resources)
 *
 * The player moves with the D-pad on pad 1. Eight enemies chase the player
 * at a constant speed along a normalized direction vector. Collecting gems
 * scores points. Touching an enemy ends the game. Press START to restart.
 *
 * Performance notes:
 *  - Positions use s32 8.8 fixed point. No floats are used.
 *  - Integer pixel positions are cached once per frame / per move, so the
 *    32-bit shifts are not repeated in aiming, collision and sprite code.
 *  - Each enemy re-aims every 4 frames, staggered so 2 enemies re-aim per frame.
 *    Re-aiming uses a fast length approximation and one 68000 DIVS per axis.
 *  - The HUD text is redrawn only when its value changes.
 *  - Sprite static fields (size, attributes, links) are written once; per frame
 *    only X/Y go into the SGDK sprite cache, which is sent by DMA only on
 *    frames where something moved.
 */

#include <genesis.h>

/* ------------------------------------------------------------------------- */
/* Tiles (4bpp, 8 x u32 per tile, one nibble per pixel)                       */
/* ------------------------------------------------------------------------- */

static const u32 tile_player[8] =
{
    0x00222200,
    0x02222220,
    0x22233222,
    0x22333322,
    0x22333322,
    0x22233222,
    0x02222220,
    0x00222200
};

static const u32 tile_enemy[8] =
{
    0x40000004,
    0x04444440,
    0x04544540,
    0x04444440,
    0x04455440,
    0x04444440,
    0x04400440,
    0x40000004
};

static const u32 tile_gem[8] =
{
    0x00066000,
    0x00666600,
    0x06677660,
    0x66777766,
    0x66777766,
    0x06677660,
    0x00666600,
    0x00066000
};

static const u32 tile_wall[8] =
{
    0x88888889,
    0x88888889,
    0x88888889,
    0x99999999,
    0x88898888,
    0x88898888,
    0x88898888,
    0x99999999
};

static const u32 tile_floor[8] =
{
    0xAAAAAAAA,
    0xAAAAAAAA,
    0xAAAAAAAA,
    0xAAABAAAA,
    0xAAAAAAAA,
    0xAAAAAAAA,
    0xAAAAAAAA,
    0xAAAAAAAB
};

#define T_PLAYER    (TILE_USER_INDEX + 0)
#define T_ENEMY     (TILE_USER_INDEX + 1)
#define T_GEM       (TILE_USER_INDEX + 2)
#define T_WALL      (TILE_USER_INDEX + 3)
#define T_FLOOR     (TILE_USER_INDEX + 4)

/* ------------------------------------------------------------------------- */
/* Game constants                                                             */
/* ------------------------------------------------------------------------- */

#define NUM_ENEMIES     8

#define FP_SHIFT        8                   /* 8.8 fixed point */
#define TO_FP(v)        ((s32)(v) << FP_SHIFT)
#define FROM_FP(v)      ((s16)((v) >> FP_SHIFT))

#define PLAYER_SPEED    512                 /* 2.0 px/frame */
#define PLAYER_DIAG     362                 /* 2.0 * 0.7071 */
#define ENEMY_SPEED     192                 /* 0.75 px/frame */

/* Arena interior in pixels (sprite top-left limits, 8x8 sprites) */
#define ARENA_MIN_X     8
#define ARENA_MAX_X     (312 - 8)
#define ARENA_MIN_Y     16
#define ARENA_MAX_Y     (216 - 8)

#define SPR_PLAYER      0
#define SPR_GEM         1
#define SPR_ENEMY0      2
#define SPR_COUNT       (SPR_ENEMY0 + NUM_ENEMIES)

#define SPR_HW_OFS      0x80                /* VDP sprite coordinate offset */

/* ------------------------------------------------------------------------- */
/* State                                                                      */
/* ------------------------------------------------------------------------- */

typedef struct
{
    s32 x, y;       /* 8.8 fixed point */
    s16 vx, vy;     /* 8.8 fixed point, per frame */
    s16 sx, sy;     /* cached integer pixel position (FROM_FP(x), FROM_FP(y)) */
} Enemy;            /* 16 bytes */

static s32 playerX, playerY;
static s16 playerPX, playerPY;      /* cached FROM_FP(playerX / playerY) */
static s16 gemX, gemY;
static Enemy enemies[NUM_ENEMIES];

static u32 score;
static u16 seconds;
static u16 frameCounter;
static u16 fps;
static u16 frameNum;
static bool gameOver;

static u32 shownScore;
static u16 shownSeconds;

static u16 prevJoy;

/* ------------------------------------------------------------------------- */
/* Helpers                                                                    */
/* ------------------------------------------------------------------------- */

static inline s16 iabs16(s16 v)
{
    return (v < 0) ? -v : v;
}

/* Signed 32/16 division using the native 68000 DIVS.W.
 * The caller must make sure the quotient fits in s16. */
static inline s16 fastDivs(s32 num, s16 den)
{
    __asm__ ("divs.w %2, %0" : "=d"(num) : "0"(num), "dmi"(den) : "cc");
    return (s16) num;
}

/* Approximate vector length: max*0.961 + min*0.398 (error ~4%) */
static inline s16 approxLen(s16 ax, s16 ay)
{
    s16 mx, mn;
    if (ax > ay) { mx = ax; mn = ay; }
    else         { mx = ay; mn = ax; }
    return (s16)((((s32)mx * 123) + ((s32)mn * 51)) >> 7);
}

static u16 randRange(u16 range)
{
    return random() % range;
}

static inline void syncPlayerPixels(void)
{
    playerPX = FROM_FP(playerX);
    playerPY = FROM_FP(playerY);
}

/* ------------------------------------------------------------------------- */
/* Enemy logic                                                                */
/* ------------------------------------------------------------------------- */

static inline void aimEnemy(Enemy *e)
{
    const s16 dx = playerPX - e->sx;
    const s16 dy = playerPY - e->sy;
    const s16 len = approxLen(iabs16(dx), iabs16(dy));

    if (len == 0)
    {
        e->vx = 0;
        e->vy = 0;
        return;
    }

    /* Normalized direction * speed (8.8). |dx*SPEED/len| <= ~200, fits s16. */
    e->vx = fastDivs((s32)dx * ENEMY_SPEED, len);
    e->vy = fastDivs((s32)dy * ENEMY_SPEED, len);
}

static void spawnEnemy(Enemy *e)
{
    const s16 px = playerPX;
    const s16 py = playerPY;
    s16 x = ARENA_MIN_X, y = ARENA_MIN_Y;
    u16 tries;

    for (tries = 0; tries < 12; tries++)
    {
        switch (random() & 3)
        {
            case 0: /* top */
                x = ARENA_MIN_X + randRange(ARENA_MAX_X - ARENA_MIN_X + 1);
                y = ARENA_MIN_Y;
                break;
            case 1: /* bottom */
                x = ARENA_MIN_X + randRange(ARENA_MAX_X - ARENA_MIN_X + 1);
                y = ARENA_MAX_Y;
                break;
            case 2: /* left */
                x = ARENA_MIN_X;
                y = ARENA_MIN_Y + randRange(ARENA_MAX_Y - ARENA_MIN_Y + 1);
                break;
            default: /* right */
                x = ARENA_MAX_X;
                y = ARENA_MIN_Y + randRange(ARENA_MAX_Y - ARENA_MIN_Y + 1);
                break;
        }

        if ((iabs16(x - px) + iabs16(y - py)) >= 112) break;
    }

    e->x = TO_FP(x);
    e->y = TO_FP(y);
    e->sx = x;
    e->sy = y;
    aimEnemy(e);
}

static void spawnGem(void)
{
    const s16 px = playerPX;
    const s16 py = playerPY;
    u16 tries;

    for (tries = 0; tries < 12; tries++)
    {
        gemX = ARENA_MIN_X + 8 + randRange(ARENA_MAX_X - ARENA_MIN_X - 16);
        gemY = ARENA_MIN_Y + 8 + randRange(ARENA_MAX_Y - ARENA_MIN_Y - 16);
        if ((iabs16(gemX - px) + iabs16(gemY - py)) >= 64) break;
    }
}

/* ------------------------------------------------------------------------- */
/* HUD                                                                        */
/* ------------------------------------------------------------------------- */

static void drawScore(void)
{
    char buf[8];
    uintToStr(score, buf, 5);
    VDP_drawText(buf, 7, 0);
    shownScore = score;
}

static void drawTime(void)
{
    char buf[8];
    uintToStr(seconds, buf, 4);
    VDP_drawText(buf, 35, 0);
    shownSeconds = seconds;
}

static void drawHudStatic(void)
{
    VDP_clearTextLine(0);
    VDP_drawText("SCORE", 1, 0);
    VDP_drawText("TIME", 30, 0);
    drawScore();
    drawTime();
}

/* ------------------------------------------------------------------------- */
/* Setup                                                                      */
/* ------------------------------------------------------------------------- */

static void setupPalettes(void)
{
    /* PAL0: background color and text */
    PAL_setColor(0,  RGB24_TO_VDPCOLOR(0x000000));
    PAL_setColor(15, RGB24_TO_VDPCOLOR(0xFFFFFF));

    /* PAL1: game graphics (entries 16..31) */
    PAL_setColor(16 + 2,  RGB24_TO_VDPCOLOR(0x00C0FF)); /* player body */
    PAL_setColor(16 + 3,  RGB24_TO_VDPCOLOR(0xFFFFFF)); /* player core */
    PAL_setColor(16 + 4,  RGB24_TO_VDPCOLOR(0xE02020)); /* enemy body */
    PAL_setColor(16 + 5,  RGB24_TO_VDPCOLOR(0xFFE000)); /* enemy eyes */
    PAL_setColor(16 + 6,  RGB24_TO_VDPCOLOR(0x20E040)); /* gem */
    PAL_setColor(16 + 7,  RGB24_TO_VDPCOLOR(0xC0FFC0)); /* gem shine */
    PAL_setColor(16 + 8,  RGB24_TO_VDPCOLOR(0x806040)); /* wall */
    PAL_setColor(16 + 9,  RGB24_TO_VDPCOLOR(0x403020)); /* wall mortar */
    PAL_setColor(16 + 10, RGB24_TO_VDPCOLOR(0x202040)); /* floor */
    PAL_setColor(16 + 11, RGB24_TO_VDPCOLOR(0x404060)); /* floor dots */
}

static void setupTiles(void)
{
    VDP_loadTileData(tile_player, T_PLAYER, 1, CPU);
    VDP_loadTileData(tile_enemy,  T_ENEMY,  1, CPU);
    VDP_loadTileData(tile_gem,    T_GEM,    1, CPU);
    VDP_loadTileData(tile_wall,   T_WALL,   1, CPU);
    VDP_loadTileData(tile_floor,  T_FLOOR,  1, CPU);
}

/* Write the constant sprite fields (size, attributes, link) once.
 * Per-frame code only touches X/Y in the sprite cache afterwards. */
static void setupSprites(void)
{
    u16 i;

    VDP_setSpriteFull(SPR_PLAYER, 0, 0,
                      SPRITE_SIZE(1, 1),
                      TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, T_PLAYER),
                      SPR_PLAYER + 1);

    VDP_setSpriteFull(SPR_GEM, 0, 0,
                      SPRITE_SIZE(1, 1),
                      TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, T_GEM),
                      SPR_GEM + 1);

    for (i = 0; i < NUM_ENEMIES; i++)
    {
        const u16 idx = SPR_ENEMY0 + i;
        const u8 link = (idx + 1 < SPR_COUNT) ? (idx + 1) : 0;

        VDP_setSpriteFull(idx, 0, 0,
                          SPRITE_SIZE(1, 1),
                          TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, T_ENEMY),
                          link);
    }
}

static void drawArena(void)
{
    const u16 wall  = TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, T_WALL);
    const u16 floor = TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, T_FLOOR);

    /* Row 0 is reserved for the HUD. The arena uses rows 1..27. */
    VDP_fillTileMapRect(BG_B, floor, 1, 2, 38, 25);
    VDP_fillTileMapRect(BG_B, wall, 0, 1, 40, 1);   /* top    */
    VDP_fillTileMapRect(BG_B, wall, 0, 27, 40, 1);  /* bottom */
    VDP_fillTileMapRect(BG_B, wall, 0, 2, 1, 25);   /* left   */
    VDP_fillTileMapRect(BG_B, wall, 39, 2, 1, 25);  /* right  */
}

static void resetGame(void)
{
    u16 i;

    playerX = TO_FP(156);
    playerY = TO_FP(108);
    syncPlayerPixels();

    for (i = 0; i < NUM_ENEMIES; i++)
        spawnEnemy(&enemies[i]);

    spawnGem();

    score = 0;
    seconds = 0;
    frameCounter = 0;
    gameOver = FALSE;

    VDP_clearTextLine(14);
    drawHudStatic();
}

/* ------------------------------------------------------------------------- */
/* Per-frame update                                                           */
/* ------------------------------------------------------------------------- */

static void updatePlayer(u16 joy)
{
    s16 mx = 0, my = 0;
    s16 spd;

    if (joy & BUTTON_LEFT)       mx = -1;
    else if (joy & BUTTON_RIGHT) mx = 1;
    if (joy & BUTTON_UP)         my = -1;
    else if (joy & BUTTON_DOWN)  my = 1;

    spd = (mx && my) ? PLAYER_DIAG : PLAYER_SPEED;

    if (mx < 0)      playerX -= spd;
    else if (mx > 0) playerX += spd;
    if (my < 0)      playerY -= spd;
    else if (my > 0) playerY += spd;

    if (playerX < TO_FP(ARENA_MIN_X)) playerX = TO_FP(ARENA_MIN_X);
    if (playerX > TO_FP(ARENA_MAX_X)) playerX = TO_FP(ARENA_MAX_X);
    if (playerY < TO_FP(ARENA_MIN_Y)) playerY = TO_FP(ARENA_MIN_Y);
    if (playerY > TO_FP(ARENA_MAX_Y)) playerY = TO_FP(ARENA_MAX_Y);

    syncPlayerPixels();
}

static void updateEnemies(void)
{
    const s16 px = playerPX;
    const s16 py = playerPY;
    const u16 slot = frameNum & 3;
    Enemy *e = enemies;
    u16 i;

    for (i = 0; i < NUM_ENEMIES; i++, e++)
    {
        s16 ex, ey;

        /* Staggered re-aim: each enemy every 4 frames, 2 per frame */
        if ((i & 3) == slot) aimEnemy(e);

        e->x += e->vx;
        e->y += e->vy;

        ex = FROM_FP(e->x);
        ey = FROM_FP(e->y);
        e->sx = ex;
        e->sy = ey;

        /* Collision with player (8x8 boxes, slightly forgiving) */
        if ((iabs16(ex - px) < 6) && (iabs16(ey - py) < 6))
            gameOver = TRUE;
    }
}

static void updateGem(void)
{
    if ((iabs16(gemX - playerPX) < 8) && (iabs16(gemY - playerPY) < 8))
    {
        score += 10;
        spawnGem();
    }
}

static void updateTimer(void)
{
    if (++frameCounter >= fps)
    {
        frameCounter = 0;
        seconds++;
        score += 1;     /* survival bonus */
    }
}

static void updateSprites(void)
{
    VDPSprite *s = vdpSpriteCache;
    const Enemy *e = enemies;
    u16 i;

    s->x = playerPX + SPR_HW_OFS;
    s->y = playerPY + SPR_HW_OFS;
    s++;

    s->x = gemX + SPR_HW_OFS;
    s->y = gemY + SPR_HW_OFS;
    s++;

    for (i = 0; i < NUM_ENEMIES; i++, s++, e++)
    {
        s->x = e->sx + SPR_HW_OFS;
        s->y = e->sy + SPR_HW_OFS;
    }

    VDP_updateSprites(SPR_COUNT, DMA_QUEUE);
}

/* ------------------------------------------------------------------------- */
/* Main                                                                       */
/* ------------------------------------------------------------------------- */

int main(bool hardReset)
{
    (void) hardReset;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();
    VDP_setTextPlane(BG_A);
    VDP_setTextPalette(PAL0);

    JOY_init();

    fps = SYS_isPAL() ? 50 : 60;

    setupPalettes();
    setupTiles();
    setupSprites();
    drawArena();

    setRandomSeed(GET_VCOUNTER ^ 0x5A3C);

    frameNum = 0;
    prevJoy = 0;
    resetGame();

    while (TRUE)
    {
        const u16 joy = JOY_readJoypad(JOY_1);
        const u16 pressed = joy & ~prevJoy;
        bool spritesDirty = FALSE;
        prevJoy = joy;

        if (!gameOver)
        {
            updatePlayer(joy);
            updateEnemies();
            updateGem();
            updateTimer();
            spritesDirty = TRUE;

            if (gameOver)
                VDP_drawText("GAME OVER - PRESS START", 8, 14);
        }
        else if (pressed & BUTTON_START)
        {
            resetGame();
            spritesDirty = TRUE;
        }

        /* HUD: redraw only on change */
        if (score != shownScore)     drawScore();
        if (seconds != shownSeconds) drawTime();

        /* Sprite table in VRAM stays valid while nothing moves (game over) */
        if (spritesDirty) updateSprites();

        frameNum++;
        SYS_doVBlankProcess();
    }

    return 0;
}
