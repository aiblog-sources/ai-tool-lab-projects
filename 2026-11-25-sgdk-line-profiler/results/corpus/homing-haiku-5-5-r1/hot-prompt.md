Here is a Mega Drive game written with SGDK 2.09. Make it run faster on the real hardware (68000 at 7.67 MHz) without changing what the game does or how it looks. Reply with the full main.c in one ```c code block.

Below is a line profile of this code, measured in an emulator with a scripted pad: cycles per video frame by source line (one NTSC frame is 128,005 cycles), what each line called, and the findings of a performance lint on the same lines.

== Lines by measured cost (sgdkhot: mdlines + sgdklint) ==
budget 128,005 cycles per video frame (NTSC); 1,200 frames counted; busy 36,201 cycles per frame on average (28.3%)
each line = its own instructions + every library/libgcc call made from it (game helpers count on their own lines)
    5,502 /frame  15.2% of busy  src/main.c:107  e->x += dx * ENEMY_SPEED / (s32)dist;
                     4,616 in __divsi3: 7.93 calls/frame, 582 cycles each
                 lint [div] guarded: '/' with an operand of unknown type: 453 to 1,062 cycles unless both operands are u16. Here: 8 passes x 453 = 
    5,461 /frame  15.1% of busy  src/main.c:108  e->y += dy * ENEMY_SPEED / (s32)dist;
                     4,609 in __divsi3: 7.93 calls/frame, 581 cycles each
                 lint [div] guarded: '/' with an operand of unknown type: 453 to 1,062 cycles unless both operands are u16. Here: 8 passes x 453 = 
    5,106 /frame  14.1% of busy  src/main.c:99  u32 dist = isqrt32((u32)(dx * dx + dy * dy));
                     4,159 in __mulsi3: 15.87 calls/frame, 262 cycles each
                 lint [mul32] hot: 32 x 32-bit multiply: 268 cycles (__mulsi3, s32 * s32 and s16 * s32 measured the same); a muls.w on two s16 is
    3,430 /frame   9.5% of busy  src/main.c:247  SYS_doVBlankProcess();
                     3,358 in SYS_doVBlankProcessEx.constprop.0.isra.0: 0.99 calls/frame, 3,386 cycles each
    3,296 /frame   9.1% of busy  src/main.c:138  VDP_updateSprites(NUM_ENEMIES + 1, DMA);
                     3,163 in DMA_transfer.isra.0: 0.99 calls/frame, 3,189 cycles each
    2,438 /frame   6.7% of busy  src/main.c:65  if (n >= res + bit) {
    1,272 /frame   3.5% of busy  src/main.c:67  res = (res >> 1) + bit;
    1,247 /frame   3.4% of busy  src/main.c:63  while (bit > n) bit >>= 2;
    1,078 /frame   3.0% of busy  src/main.c:71  bit >>= 2;
    1,012 /frame   2.8% of busy  src/main.c:64  while (bit) {
      711 /frame   2.0% of busy  src/main.c:125  VDP_setSprite(i,
      576 /frame   1.6% of busy  src/main.c:151  p[i - 1] = (char)('0' + (v % 10));
                       528 in __umodsi3: 0.63 calls/frame, 833 cycles each
                 lint [div] guarded: '%' with an operand of unknown type: 453 to 1,062 cycles if the other operand is 32-bit or signed, 117 if both
      489 /frame   1.3% of busy  src/main.c:236  u32 secs = frames / fps;
                       408 in __udivsi3: 0.99 calls/frame, 411 cycles each
                 lint [div] frame: '/' on 32-bit / 32-bit: 617 cycles (s32 / s32); SGDK divu()/divs() is 133-139, a shift is 23
      378 /frame   1.0% of busy  src/main.c:113  if (ABSV(dx) < HIT_RANGE && ABSV(dy) < HIT_RANGE) {
      375 /frame   1.0% of busy  src/main.c:66  n -= res + bit;

== Lint findings, by measured cost ==
   matters     5,502 /frame (4.30% of a frame)  src/main.c:107 [div] guarded
   matters     5,461 /frame (4.27% of a frame)  src/main.c:108 [div] guarded
   matters     5,106 /frame (3.99% of a frame)  src/main.c:99 [mul32] hot
     small       576 /frame (0.45% of a frame)  src/main.c:151 [div] guarded
     small       488 /frame (0.38% of a frame)  src/main.c:236 [div] frame
     small       298 /frame (0.23% of a frame)  src/main.c:152 [div] guarded
negligible       183 /frame (0.14% of a frame)  src/main.c:165 [text] guarded
negligible       147 /frame (0.11% of a frame)  src/main.c:170 [text] guarded
negligible        90 /frame (0.07% of a frame)  src/main.c:163 [div] guarded
negligible        58 /frame (0.05% of a frame)  src/main.c:168 [div] guarded
negligible        16 /frame (0.01% of a frame)  src/main.c:80 [div] guarded
negligible        16 /frame (0.01% of a frame)  src/main.c:79 [div] guarded
verdicts: matters >= 1% of a frame, small >= 0.2%, negligible below, not run = no cycles on that line in this run

== Game lines over 2% of a frame with no lint finding ==
    3,430 /frame  src/main.c:247 (main), mostly SYS_doVBlankProcessEx.constprop.0.isra.0
    3,296 /frame  src/main.c:138 (updateSprites), mostly DMA_transfer.isra.0

library code with no game line on the stack (interrupt handlers, SGDK's VBlank work): 318 cycles per frame

main.c:
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
