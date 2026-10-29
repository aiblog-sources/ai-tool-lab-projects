/*
 * main.c - Particle explosion demo for SGDK 2.09 (Sega Mega Drive / Genesis)
 *
 * Press A on pad 1 to spawn an explosion of 48 particle sprites.
 * The particles fly out in all directions, fall under gravity, shrink
 * as they age, and disappear after 1 second.
 *
 * No resource files are used. The tiles are const u32 arrays loaded
 * with VDP_loadTileData. The hardware sprite table is written directly
 * through the SGDK sprite cache and uploaded with VDP_updateSprites.
 *
 * Performance notes (68000 @ 7.67 MHz):
 *  - Positions are stored in 16.16 fixed point (the old 24.8 values
 *    scaled by 256), so the integer pixel is just the high word. This
 *    replaces the costly 32-bit "asr.l #8" with a cheap "swap". The
 *    results are bit-identical to the old 8-bit fixed point math.
 *  - Velocities are stored with the same scaling, so the per-frame
 *    adds stay simple 32-bit adds.
 *  - cos/sin come from full 48-entry tables. This removes the "% 48"
 *    division and the branches.
 *  - "random() % 512" becomes "random() & 511". This is identical
 *    because random() returns a u16.
 *  - The multiply uses 16x16->32 (muls.w) instead of a 32x32 libcall.
 *  - Update and draw are merged into one pass over the particles.
 *  - The sprite cache is written directly. This avoids one function
 *    call per sprite.
 *  - The life thresholds are computed once, not divided every frame.
 *  - When no particles are alive, the work is skipped. The parked
 *    sprite is uploaded only once.
 */

#include <genesis.h>

#define NUM_PARTICLES   48

/* Original fixed point: 8 fractional bits (used for velocity math) */
#define FP_SHIFT        8

#define GRAVITY         28      /* ~0.11 px/frame^2 */
#define UP_BIAS         200     /* initial upward push, ~0.78 px/frame */
#define SPEED_MIN       384     /* 1.5 px/frame */
#define SPEED_RANGE     512     /* +0..2 px/frame */

/* Internal 16.16 representation (= old 24.8 value << 8) */
#define GRAVITY_HI      ((s32)GRAVITY << 8)
#define UP_BIAS_HI      ((s32)UP_BIAS << 8)

#define SCREEN_W        320
#define SCREEN_H        224

/* ------------------------------------------------------------------ */
/* Tiles (4bpp, one u32 per row, leftmost pixel in the high nibble)    */
/* ------------------------------------------------------------------ */
static const u32 particleTiles[3 * 8] =
{
    /* Tile 0: big bright spark */
    0x00033000,
    0x00322300,
    0x03211230,
    0x32111123,
    0x32111123,
    0x03211230,
    0x00322300,
    0x00033000,

    /* Tile 1: medium spark */
    0x00000000,
    0x00033000,
    0x00322300,
    0x03222230,
    0x03222230,
    0x00322300,
    0x00033000,
    0x00000000,

    /* Tile 2: small dying ember */
    0x00000000,
    0x00000000,
    0x00044000,
    0x00433400,
    0x00433400,
    0x00044000,
    0x00000000,
    0x00000000
};

#define PARTICLE_TILE_BASE  TILE_USER_INDEX

#define ATTR_TILE0  TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, PARTICLE_TILE_BASE + 0)
#define ATTR_TILE1  TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, PARTICLE_TILE_BASE + 1)
#define ATTR_TILE2  TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, PARTICLE_TILE_BASE + 2)

/* ------------------------------------------------------------------ */
/* Direction tables: cos / sin of (k * 7.5 deg) * 256, k = 0..47       */
/* (same values as the original cos48()/sin48() functions)             */
/* ------------------------------------------------------------------ */
static const s16 cosTab[NUM_PARTICLES] =
{
     256,  254,  247,  237,  222,  203,  181,  156,
     128,   98,   66,   33,    0,  -33,  -66,  -98,
    -128, -156, -181, -203, -222, -237, -247, -254,
    -256, -254, -247, -237, -222, -203, -181, -156,
    -128,  -98,  -66,  -33,    0,   33,   66,   98,
     128,  156,  181,  203,  222,  237,  247,  254
};

static const s16 sinTab[NUM_PARTICLES] =
{
       0,   33,   66,   98,  128,  156,  181,  203,
     222,  237,  247,  254,  256,  254,  247,  237,
     222,  203,  181,  156,  128,   98,   66,   33,
       0,  -33,  -66,  -98, -128, -156, -181, -203,
    -222, -237, -247, -254, -256, -254, -247, -237,
    -222, -203, -181, -156, -128,  -98,  -66,  -33
};

/* ------------------------------------------------------------------ */
/* Particles                                                           */
/* ------------------------------------------------------------------ */
typedef struct
{
    s32 x, y;       /* position, 16.16 fixed point (pixel = high word) */
    s32 vx, vy;     /* velocity, same scaling */
    s16 life;       /* frames remaining, 0 = dead */
} Particle;

static Particle particles[NUM_PARTICLES];
static s16 lifeFrames;
static s16 lifeThird;       /* lifeFrames / 3 */
static s16 lifeTwoThirds;   /* (lifeFrames / 3) * 2 */
static u16 aliveCount;      /* number of live particles after the last frame */
static bool parked;         /* the parked sprite is already uploaded */

static void spawnExplosion(s16 cx, s16 cy)
{
    const s32 x0 = (s32)cx << 16;
    const s32 y0 = (s32)cy << 16;
    Particle *p = particles;

    for (u16 i = 0; i < NUM_PARTICLES; i++, p++)
    {
        s16 speed = SPEED_MIN + (s16)(random() & (SPEED_RANGE - 1));

        /* 16x16 -> 32 signed multiply (muls.w) */
        s32 px = (s32)cosTab[i] * (s32)speed;
        s32 py = (s32)sinTab[i] * (s32)speed;

        p->x = x0;
        p->y = y0;
        /* ((prod >> 8) << 8) == prod & ~0xFF : same floor as before */
        p->vx = px & ~0xFFL;
        p->vy = (py & ~0xFFL) - UP_BIAS_HI;
        /* Small random lifetime spread so the explosion doesn't vanish in one frame */
        p->life = lifeFrames - (s16)(random() & 3);
    }

    aliveCount = NUM_PARTICLES;
    parked = FALSE;
}

static void parkSprites(void)
{
    if (parked) return;

    /* Keep one sprite in the table, parked off-screen */
    VDPSprite *spr = vdpSpriteCache;
    spr->y = -32 + 0x80;
    spr->size = SPRITE_SIZE(1, 1);
    spr->link = 0;
    spr->attribut = ATTR_TILE0;
    spr->x = -32 + 0x80;

    VDP_updateSprites(1, DMA_QUEUE);
    parked = TRUE;
}

/* Update physics and build the sprite table in one pass */
static void updateAndDrawParticles(void)
{
    if (aliveCount == 0)
    {
        parkSprites();
        return;
    }

    const s16 t1 = lifeThird;
    const s16 t2 = lifeTwoThirds;

    VDPSprite *spr = vdpSpriteCache;
    Particle *p = particles;
    u16 count = 0;

    for (u16 i = NUM_PARTICLES; i != 0; i--, p++)
    {
        s16 life = p->life;
        if (life <= 0) continue;

        s32 vy = p->vy + GRAVITY_HI;
        s32 x  = p->x + p->vx;
        s32 y  = p->y + vy;
        p->vy = vy;
        p->x  = x;
        p->y  = y;
        life--;

        s16 sx = (s16)(x >> 16);
        s16 sy = (s16)(y >> 16);

        /* Kill particles that left the screen */
        if (sx < -8 || sx > SCREEN_W + 8 || sy > SCREEN_H + 8)
            life = 0;

        p->life = life;
        if (life <= 0) continue;

        u16 attr;
        if (life > t2)       attr = ATTR_TILE0;
        else if (life > t1)  attr = ATTR_TILE1;
        else                 attr = ATTR_TILE2;

        count++;
        spr->y = sy - 4 + 0x80;
        spr->size = SPRITE_SIZE(1, 1);
        spr->link = (u8)count;
        spr->attribut = attr;
        spr->x = sx - 4 + 0x80;
        spr++;
    }

    aliveCount = count;

    if (count == 0)
    {
        parkSprites();
        return;
    }

    /* Terminate the link chain */
    spr[-1].link = 0;
    VDP_updateSprites(count, DMA_QUEUE);
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */
int main(bool hardReset)
{
    (void)hardReset;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    /* 1 second of lifetime, whatever the video standard */
    lifeFrames = IS_PAL_SYSTEM ? 50 : 60;
    lifeThird = lifeFrames / 3;
    lifeTwoThirds = lifeThird * 2;

    /* Palettes */
    PAL_setColor(0,       RGB24_TO_VDPCOLOR(0x000018));  /* background: dark blue */
    PAL_setColor(16 + 1,  RGB24_TO_VDPCOLOR(0xFFFFC0));  /* core: pale yellow */
    PAL_setColor(16 + 2,  RGB24_TO_VDPCOLOR(0xFFA020));  /* orange */
    PAL_setColor(16 + 3,  RGB24_TO_VDPCOLOR(0xE02000));  /* red */
    PAL_setColor(16 + 4,  RGB24_TO_VDPCOLOR(0x601000));  /* dark red */

    /* Tiles */
    VDP_loadTileData(particleTiles, PARTICLE_TILE_BASE, 3, CPU);

    /* Clear particles and sprite table */
    for (u16 i = 0; i < NUM_PARTICLES; i++)
        particles[i].life = 0;
    aliveCount = 0;
    parked = FALSE;

    VDP_resetSprites();
    updateAndDrawParticles();   /* no live particles: just parks the sprite */

    VDP_drawText("PARTICLE EXPLOSION DEMO", 8, 2);
    VDP_drawText("Press A to explode!", 10, 25);

    u16 prevJoy = 0;

    while (TRUE)
    {
        u16 joy = JOY_readJoypad(JOY_1);
        u16 pressed = joy & ~prevJoy;
        prevJoy = joy;

        if (pressed & BUTTON_A)
        {
            /* Random spot in the upper-middle part of the screen */
            s16 cx = 60 + (random() % 200);   /* 60..259 */
            s16 cy = 50 + (random() % 90);    /* 50..139 */
            spawnExplosion(cx, cy);
        }

        updateAndDrawParticles();

        SYS_doVBlankProcess();
    }

    return 0;
}
