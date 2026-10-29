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

static void spawnEnemy(Vec *e)
{
    u16 side = random() & 3;
    s32 fx = (s32)(random() % (ARENA_W - SPR_PX + 1)) * ONE_PX;
    s32 fy = (s32)(random() % (ARENA_H - SPR_PX + 1)) * ONE_PX;

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
        s32 dx = player.x - e->x;
        s32 dy = player.y - e->y;
        u32 dist = isqrt32((u32)(dx * dx + dy * dy));

        if (dist <= ENEMY_SPEED) {
            /* Close enough: snap onto the player */
            e->x = player.x;
            e->y = player.y;
        } else {
            /* Normalized direction * constant speed */
            e->x += dx * ENEMY_SPEED / (s32)dist;
            e->y += dy * ENEMY_SPEED / (s32)dist;
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

static char *putNum(char *p, u32 v, u8 digits)
{
    for (u8 i = digits; i > 0; i--) {
        p[i - 1] = (char)('0' + (v % 10));
        v /= 10;
    }
    return p + digits;
}

static void drawHud(u32 score, u32 secs)
{
    char buf[24];
    char *p;

    p = putStr(buf, "SCORE ");
    p = putNum(p, score % 100000000UL, 8);
    *p = 0;
    VDP_drawText(buf, 1, 1);

    p = putStr(buf, "TIME ");
    p = putNum(p, secs % 10000UL, 4);
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

    const u32 fps = IS_PAL_SYSTEM ? 50 : 60;
    u32 score = 0;
    u32 frames = 0;
    u32 lastScore = 0xFFFFFFFFUL;
    u32 lastSecs  = 0xFFFFFFFFUL;

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
        score += updateEnemies();
        frames++;

        u32 secs = frames / fps;

        /* Rendering */
        updateSprites();

        if (score != lastScore || secs != lastSecs) {
            drawHud(score, secs);
            lastScore = score;
            lastSecs  = secs;
        }

        SYS_doVBlankProcess();
    }

    return 0;
}
