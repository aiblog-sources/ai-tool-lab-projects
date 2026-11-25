I rewrote the hot paths: the square root, the distance normalization, and the HUD digit extraction. Game logic, random call order, and drawing are unchanged. I have not compiled or run this, so check it against your emulator run before trusting it.

```c
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

/* ---------------- Hardware arithmetic ----------------
 * Direct 68000 MULS.W / DIVS.W / DIVU.W instead of libgcc's __mulsi3,
 * __divsi3 and __udivsi3. Each caller guarantees the operand ranges below.
 */

/* a * b for 16-bit signed operands, full 32-bit result */
static inline s32 hw_muls16(s16 a, s16 b)
{
    s32 r = a;
    __asm__ ("muls.w %1,%0" : "+d" (r) : "dmi" (b) : "cc");
    return r;
}

/* a / b (truncating) for 32-bit signed a and 16-bit signed b; quotient must fit s16 */
static inline s16 hw_divs32_16(s32 a, s16 b)
{
    __asm__ ("divs.w %1,%0" : "+d" (a) : "dmi" (b) : "cc");
    return (s16)a;
}

/* 32-bit unsigned a / 16-bit b; quotient must fit 16 bits.
 * Result: quotient in low word, remainder in high word. */
static inline u32 hw_divu32_16(u32 a, u16 b)
{
    __asm__ ("divu.w %1,%0" : "+d" (a) : "dmi" (b) : "cc");
    return a;
}

/* v / 10 for any u32; stores v % 10 in *digit, returns the quotient */
static u32 div10(u32 v, u8 *digit)
{
    u32 hi = hw_divu32_16(v >> 16, 10);   /* high part: quotient, remainder r1 */
    u32 lo = hw_divu32_16(((hi >> 16) << 16) | (v & 0xFFFFUL), 10);
    *digit = (u8)(lo >> 16);
    return ((hi & 0xFFFFUL) << 16) | (lo & 0xFFFFUL);
}

/* ---------------- Helpers ---------------- */

/* Exact floor(sqrt(n)) by digit-by-digit method, fully unrolled.
 * Valid for n < 2^26, which holds since positions stay inside the arena. */
static u32 isqrt32(u32 n)
{
    u32 res = 0;

#define ISQRT_STEP(bit) do {                          \
        u32 t_ = res + (bit);                         \
        if (n >= t_) { n -= t_; res = (res >> 1) + (bit); } \
        else         { res >>= 1; }                   \
    } while (0)

    ISQRT_STEP(1UL << 24);
    ISQRT_STEP(1UL << 22);
    ISQRT_STEP(1UL << 20);
    ISQRT_STEP(1UL << 18);
    ISQRT_STEP(1UL << 16);
    ISQRT_STEP(1UL << 14);
    ISQRT_STEP(1UL << 12);
    ISQRT_STEP(1UL << 10);
    ISQRT_STEP(1UL << 8);
    ISQRT_STEP(1UL << 6);
    ISQRT_STEP(1UL << 4);
    ISQRT_STEP(1UL << 2);
    ISQRT_STEP(1UL << 0);

#undef ISQRT_STEP
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

        /* |dx|, |dy| <= arena size in 1/16 px (under 4864), so they fit in s16 */
        u32 dist = isqrt32((u32)(hw_muls16((s16)dx, (s16)dx) +
                                 hw_muls16((s16)dy, (s16)dy)));

        if (dist <= ENEMY_SPEED) {
            /* Close enough: snap onto the player */
            e->x = player.x;
            e->y = player.y;
        } else {
            /* Normalized direction * constant speed.
             * |dx * ENEMY_SPEED| <= 116736 and dist >= 25, so the quotient fits s16. */
            e->x += hw_divs32_16(dx * ENEMY_SPEED, (s16)dist);
            e->y += hw_divs32_16(dy * ENEMY_SPEED, (s16)dist);
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

/* Writes the low `digits` decimal digits of v */
static char *putNum(char *p, u32 v, u8 digits)
{
    for (u8 i = digits; i > 0; i--) {
        u8 d;
        v = div10(v, &d);
        p[i - 1] = (char)('0' + d);
    }
    return p + digits;
}

static void drawHud(u32 score, u32 secs)
{
    char buf[24];
    char *p;

    p = putStr(buf, "SCORE ");
    p = putNum(p, score, 8);      /* 8 low digits == score % 100000000 */
    *p = 0;
    VDP_drawText(buf, 1, 1);

    p = putStr(buf, "TIME ");
    p = putNum(p, secs, 4);       /* 4 low digits == secs % 10000 */
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
    u32 secs = 0;       /* always equals frames / fps */
    u32 subSec = 0;     /* always equals frames % fps */
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

        /* Incremental frames / fps, so no per-frame division */
        if (frames == 0) {              /* u32 wrap: frames / fps restarts at 0 */
            secs = 0;
            subSec = 0;
        } else if (++subSec == fps) {
            subSec = 0;
            secs++;
        }

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
```

**What changed**

- **Distance normalization (lines 107–108, the largest cost):** `dx * ENEMY_SPEED / dist` now uses a single `DIVS.W`, not `__divsi3`. The quotient is at most about 4669, so it fits in 16 bits.
- **Squared distance (line 99):** `dx*dx` and `dy*dy` use `MULS.W`, since both operands fit in s16.
- **Square root (lines 63–71):** The normalization loop is gone and the 13 steps are unrolled. The result is identical because leading steps with `bit > n` do nothing.
- **HUD digits (lines 151–152):** Each digit uses two `DIVU.W` operations instead of two libgcc division calls. The `% 100000000` and `% 10000` are removed because extracting only the low N digits already gives the same output.
- **Seconds counter (line 236):** Tracked incrementally with `secs`/`subSec`, with an explicit reset on u32 wrap so it matches `frames / fps` exactly.

**Assumptions to check**

- The optimizations rely on enemies and the player staying inside the arena. That keeps `|dx|, |dy| ≤ 4864` and the squared distance below 2^26. Both hold given the current movement and clamping.
- The inline assembly uses GNU m68k syntax. If your toolchain rejects it, let me know the error.

**Not changed:** `SYS_doVBlankProcess` and the sprite DMA (about 6.7k cycles per frame together) are SGDK internals. Moving the DMA to a queued mode would shift when the sprite table is written, so I left it alone.