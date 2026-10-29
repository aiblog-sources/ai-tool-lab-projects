#include <genesis.h>

/* ---------------- Config ---------------- */
#define ARENA_W       320
#define ARENA_H       224
#define SPR_PX        16                 /* entities are 2x2 tiles = 16x16 px */
#define NUM_ENEMIES   8

#define SUB           4                  /* positions are stored in 1/16 px */
#define ONE_PX        (1 << SUB)
#define PLAYER_SPEED  (2 * ONE_PX)       /* 2 px / frame */
#define ENEMY_SPEED   (3 * ONE_PX / 2)   /* 1.5 px / frame (constant) */
#define HIT_RANGE     (SPR_PX * ONE_PX)  /* 16 px box collision */
#define KILL_SCORE    100

#define MAX_X         ((ARENA_W - SPR_PX) * ONE_PX)
#define MAX_Y         ((ARENA_H - SPR_PX) * ONE_PX)

#define TILE_ENEMY    TILE_USER_INDEX
#define TILE_PLAYER   (TILE_USER_INDEX + 4)

#define SCORE_MOD     100000000UL       /* HUD shows score % 1e8 */
#define WRAP_REM      94967296UL        /* 2^32 mod SCORE_MOD */

#define ABSV(v)       ((v) < 0 ? -(v) : (v))

/* ---------------- Graphics (4bpp 8x8 tiles, one u32 per row) ---------------- */
/* A 16x16 entity uses four 8x8 tiles; all four are identical solid tiles.     */
static const u32 enemyTile[8] = {          /* palette index 1 = red */
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111
};

static const u32 playerTile[8] = {         /* palette index 2 = green */
    0x22222222, 0x22222222, 0x22222222, 0x22222222,
    0x22222222, 0x22222222, 0x22222222, 0x22222222
};

/* Colors are 0BGR (0000 BBB0 GGG0 RRR0) */
static const u16 palText[16] = {
    0x0000, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE,
    0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE
};

static const u16 palSprite[16] = {
    0x0000,  /* 0: transparent */
    0x000E,  /* 1: red (enemies) */
    0x00E0,  /* 2: green (player) */
    0x0EEE,  /* 3: white */
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
};

/* ---------------- State ---------------- */
typedef struct { s32 x; s32 y; } Vec;   /* 1/16 px units */

static Vec player;
static Vec enemies[NUM_ENEMIES];

/* ---------------- Helpers ---------------- */
static u32 isqrt32(u32 n)
{
    u32 res = 0;
    u32 bit = 1UL << 30;
    while (bit > n) bit >>= 2;
    while (bit) {
        if (n >= res + bit) {
            n -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return res;
}

/* Signed 32/16 division truncating toward zero (same as C).
 * Caller guarantees the quotient fits in s16. */
static s32 quotS(s32 num, s16 den)
{
    return (s16)divs(num, den);
}

static void spawnEnemy(Vec *e)
{
    u16 side = random() & 3;
    s32 fx = (s32)((u16)random() % (u16)(ARENA_W - SPR_PX + 1)) * ONE_PX;
    s32 fy = (s32)((u16)random() % (u16)(ARENA_H - SPR_PX + 1)) * ONE_PX;

    switch (side) {
        case 0:  e->x = fx;    e->y = 0;     break; /* top */
        case 1:  e->x = fx;    e->y = MAX_Y; break; /* bottom */
        case 2:  e->x = 0;     e->y = fy;    break; /* left */
        default: e->x = MAX_X; e->y = fy;    break; /* right */
    }
}

/* Moves all enemies toward the player at constant speed. Returns score gained. */
static u32 updateEnemies(void)
{
    u32 gained = 0;

    for (u8 i = 0; i < NUM_ENEMIES; i++) {
        Vec *e = &enemies[i];
        s32 dx = player.x - e->x;   /* |dx| <= MAX_X, fits in s16 */
        s32 dy = player.y - e->y;   /* |dy| <= MAX_Y, fits in s16 */
        u32 dist = isqrt32((u32)(muls((s16)dx, (s16)dx) +
                                 muls((s16)dy, (s16)dy)));

        if (dist <= ENEMY_SPEED) {
            /* Close enough: snap onto the player */
            e->x = player.x;
            e->y = player.y;
        } else {
            /* Normalized direction * constant speed (dist fits in s16 here) */
            e->x += quotS(dx * ENEMY_SPEED, (s16)dist);
            e->y += quotS(dy * ENEMY_SPEED, (s16)dist);
        }

        dx = player.x - e->x;
        dy = player.y - e->y;
        if (ABSV(dx) < HIT_RANGE && ABSV(dy) < HIT_RANGE) {
            gained += KILL_SCORE;
            spawnEnemy(e);
        }
    }

    return gained;
}

static void updateSprites(void)
{
    for (u8 i = 0; i < NUM_ENEMIES; i++) {
        VDP_setSprite(i,
                      (s16)(enemies[i].x >> SUB),
                      (s16)(enemies[i].y >> SUB),
                      SPRITE_SIZE(2, 2),
                      TILE_ATTR_FULL(PAL1, 0, FALSE, FALSE, TILE_ENEMY));
    }

    VDP_setSprite(NUM_ENEMIES,
                  (s16)(player.x >> SUB),
                  (s16)(player.y >> SUB),
                  SPRITE_SIZE(2, 2),
                  TILE_ATTR_FULL(PAL1, 0, FALSE, FALSE, TILE_PLAYER));

    VDP_updateSprites(NUM_ENEMIES + 1, DMA);
}

/* ---- HUD text ---- */
static char *putStr(char *p, const char *s)
{
    while (*s) *p++ = *s++;
    return p;
}

/* Writes the low `digits` decimal digits of v (16-bit math only). */
static char *putDigits(char *p, u16 v, u8 digits)
{
    for (u8 i = digits; i > 0; i--) {
        p[i - 1] = (char)('0' + (v % (u16)10));
        v /= (u16)10;
    }
    return p + digits;
}

/* v must be < SCORE_MOD */
static void drawScore(u32 v)
{
    char buf[16];
    char *p = putStr(buf, "SCORE ");
    u32 r = divu(v, 10000);     /* quotient in low word, remainder in high word */
    p = putDigits(p, (u16)r, 4);
    p = putDigits(p, (u16)(r >> 16), 4);
    *p = 0;
    VDP_drawText(buf, 1, 1);
}

/* v must be < 10000 */
static void drawTime(u16 v)
{
    char buf[12];
    char *p = putStr(buf, "TIME ");
    p = putDigits(p, v, 4);
    *p = 0;
    VDP_drawText(buf, 30, 1);
}

/* ---------------- Main ---------------- */
int main(bool hardReset)
{
    (void)hardReset;

    JOY_init();

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    PAL_setPalette(PAL0, palText, CPU);
    PAL_setPalette(PAL1, palSprite, CPU);

    for (u8 i = 0; i < 4; i++) {
        VDP_loadTileData(enemyTile,  TILE_ENEMY  + i, 1, CPU);
        VDP_loadTileData(playerTile, TILE_PLAYER + i, 1, CPU);
    }

    /* Title: wait for START, using the wait length as a random seed */
    VDP_clearPlane(BG_A, TRUE);
    VDP_drawText("PRESS START", 14, 13);

    u32 seed = 0;
    while (!(JOY_readJoypad(JOY_1) & BUTTON_START)) {
        seed++;
        SYS_doVBlankProcess();
    }
    setRandomSeed(seed);

    VDP_clearPlane(BG_A, TRUE);

    /* Init game state (positions in 1/16 px) */
    player.x = ((ARENA_W - SPR_PX) / 2) * ONE_PX;
    player.y = ((ARENA_H - SPR_PX) / 2) * ONE_PX;

    for (u8 i = 0; i < NUM_ENEMIES; i++) {
        spawnEnemy(&enemies[i]);
    }

    const u16 fps = IS_PAL_SYSTEM ? 50 : 60;
    u32 score = 0;            /* true score, wraps like the original u32 */
    u32 scoreDisp = 0;        /* score % SCORE_MOD, maintained incrementally */
    u32 frames = 0;           /* wraps like the original u32 frame counter */
    u16 tick = 0;             /* frames within the current second */
    u16 secsDisp = 0;         /* elapsed seconds % 10000, maintained incrementally */

    u32 lastScoreDisp = 0xFFFFFFFFUL;
    u16 lastSecsDisp  = 0xFFFF;

    while (TRUE) {
        u16 joy = JOY_readJoypad(JOY_1);

        /* Player movement */
        if (joy & BUTTON_LEFT)  player.x -= PLAYER_SPEED;
        if (joy & BUTTON_RIGHT) player.x += PLAYER_SPEED;
        if (joy & BUTTON_UP)    player.y -= PLAYER_SPEED;
        if (joy & BUTTON_DOWN)  player.y += PLAYER_SPEED;

        if (player.x < 0)     player.x = 0;
        if (player.x > MAX_X) player.x = MAX_X;
        if (player.y < 0)     player.y = 0;
        if (player.y > MAX_Y) player.y = MAX_Y;

        /* Game logic */
        u32 gained = updateEnemies();
        if (gained) {
            u32 before = score;
            score += gained;
            scoreDisp += gained;                 /* gained < SCORE_MOD */
            if (score < before) {                /* u32 wrapped once */
                scoreDisp += SCORE_MOD - WRAP_REM;
            }
            if (scoreDisp >= SCORE_MOD) scoreDisp -= SCORE_MOD;
        }

        /* Seconds: same value as frames / fps, without dividing */
        if (++frames == 0) {                     /* frame counter wrapped */
            tick = 0;
            secsDisp = 0;
        } else if (++tick >= fps) {
            tick = 0;
            if (++secsDisp >= 10000) secsDisp = 0;
        }

        /* Rendering */
        updateSprites();

        if (scoreDisp != lastScoreDisp) {
            drawScore(scoreDisp);
            lastScoreDisp = scoreDisp;
        }
        if (secsDisp != lastSecsDisp) {
            drawTime(secsDisp);
            lastSecsDisp = secsDisp;
        }

        SYS_doVBlankProcess();
    }

    return 0;
}
