The profile shows two loops over all 48 particles each frame and one library call per sprite. The changes below remove most of that work while keeping the same simulation, the same `random()` call sequence, the same sprite order and the same images.

1. **One loop instead of two.** Update and draw run in a single pass. A particle's sprite only depends on its own state after its update, so the result is the same as before.
2. **Live-particle counter.** The loop stops once it has handled every particle that was alive at the start of the frame. When nothing is alive, it skips the loop entirely.
3. **16.16 fixed point instead of 24.8.** Every stored value is the old value × 256. The pixel position becomes the high word, which is a cheap `swap` on the 68000 instead of a shift. `(x<<8)>>16 == x>>8` exactly, so every position and every off-screen test gives the same result as before. The values stay far from overflow because a particle lives at most 60 frames.
4. **Sprite table written directly.** The loop fills `vdpSpriteCache` itself instead of calling `VDP_setSpriteFull` per sprite. It writes the same fields: y/x + 0x80, size, link and attribute.
5. **Tile lookup table.** A 64-entry table built at startup maps life → tile attribute. This replaces the per-particle `third` comparisons.
6. **Cheaper tests.** `life` is never negative, so it is now unsigned and "alive" is a test against zero. The left/right screen test is a single unsigned compare.
7. **No repeated identical uploads.** Once the parked off-screen sprite has been uploaded, later idle frames skip the sprite DMA. The VRAM sprite table already holds exactly that data.

```c
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
 */

#include <genesis.h>

#define NUM_PARTICLES   48

/* Fixed point: 16 fractional bits (values are the old 8-bit FP << 8) */
#define FP_SHIFT        16
#define TO_FP(x)        ((s32)(x) << FP_SHIFT)
#define FROM_FP(x)      ((s16)((x) >> FP_SHIFT))

#define GRAVITY         (28 * 256)  /* ~0.11 px/frame^2 */
#define UP_BIAS         200         /* initial upward push, ~0.78 px/frame (8-bit FP) */
#define SPEED_MIN       384         /* 1.5 px/frame (8-bit FP) */
#define SPEED_RANGE     512         /* +0..2 px/frame (8-bit FP) */

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

/* ------------------------------------------------------------------ */
/* Direction table: cos(k * 7.5 deg) * 256 for k = 0..12               */
/* ------------------------------------------------------------------ */
static const s16 cosQuarter[13] =
{
    256, 254, 247, 237, 222, 203, 181, 156, 128, 98, 66, 33, 0
};

/* cos for 48 directions (7.5 degree steps), scaled by 256 */
static s16 cos48(u16 i)
{
    i %= 48;
    if (i <= 12) return  cosQuarter[i];
    if (i <= 24) return -cosQuarter[24 - i];
    if (i <= 36) return -cosQuarter[i - 24];
    return cosQuarter[48 - i];
}

/* sin(a) = cos(a - 90 deg) = cos(a + 270 deg) */
static s16 sin48(u16 i)
{
    return cos48(i + 36);
}

/* ------------------------------------------------------------------ */
/* Particles                                                           */
/* ------------------------------------------------------------------ */
typedef struct
{
    s32 x, y;       /* position, 16.16 fixed point */
    s32 vx, vy;     /* velocity, 16.16 fixed point */
    u16 life;       /* frames remaining, 0 = dead */
} Particle;

static Particle particles[NUM_PARTICLES];
static s16 lifeFrames;
static u16 activeCount;     /* particles with life > 0 */
static bool parked;         /* sprite table already holds the parked sprite */

#define LIFE_TABLE_SIZE 64
static u16 lifeAttr[LIFE_TABLE_SIZE];

static void buildLifeTable(void)
{
    s16 third = lifeFrames / 3;
    for (s16 l = 0; l < LIFE_TABLE_SIZE; l++)
    {
        u16 tile;
        if (l > third * 2)      tile = 0;
        else if (l > third)     tile = 1;
        else                    tile = 2;
        lifeAttr[l] = TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, PARTICLE_TILE_BASE + tile);
    }
}

static void spawnExplosion(s16 cx, s16 cy)
{
    Particle *p = particles;
    for (u16 i = 0; i < NUM_PARTICLES; i++, p++)
    {
        s32 speed = SPEED_MIN + (random() % SPEED_RANGE);

        p->x = TO_FP(cx);
        p->y = TO_FP(cy);
        p->vx = (((s32)cos48(i) * speed) >> 8) * 256;
        p->vy = ((((s32)sin48(i) * speed) >> 8) - UP_BIAS) * 256;
        /* Small random lifetime spread so the explosion doesn't vanish in one frame */
        p->life = lifeFrames - (random() & 3);
    }
    activeCount = NUM_PARTICLES;
}

static void updateAndDrawParticles(void)
{
    u16 count = 0;

    if (activeCount)
    {
        Particle *p = particles;
        VDPSprite *spr = vdpSpriteCache;
        u16 remaining = activeCount;
        u16 alive = activeCount;

        for (;; p++)
        {
            u16 life = p->life;
            if (!life) continue;

            s32 vy = p->vy + GRAVITY;
            s32 x = p->x + p->vx;
            s32 y = p->y + vy;
            p->vy = vy;
            p->x = x;
            p->y = y;
            life--;

            s16 sx = FROM_FP(x);
            s16 sy = FROM_FP(y);

            if (((u16)(sx + 8) > (SCREEN_W + 16)) || (sy > SCREEN_H + 8))
                life = 0;

            p->life = life;

            if (life)
            {
                count++;
                spr->y = sy + (0x80 - 4);
                spr->size_link = count;     /* size 1x1 (0), link = count */
                spr->attribut = lifeAttr[life];
                spr->x = sx + (0x80 - 4);
                spr++;
            }
            else
            {
                alive--;
            }

            if (--remaining == 0) break;
        }

        activeCount = alive;
    }

    if (count == 0)
    {
        if (parked) return;   /* VRAM already holds the parked sprite */

        /* Keep one sprite in the table, parked off-screen */
        VDPSprite *spr = vdpSpriteCache;
        spr->y = -32 + 0x80;
        spr->size_link = (SPRITE_SIZE(1, 1) << 8) | 0;
        spr->attribut = TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, PARTICLE_TILE_BASE);
        spr->x = -32 + 0x80;
        count = 1;
        parked = TRUE;
    }
    else
    {
        /* Terminate the link chain */
        vdpSpriteCache[count - 1].link = 0;
        parked = FALSE;
    }

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
    buildLifeTable();

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
    activeCount = 0;
    parked = FALSE;

    VDP_resetSprites();
    updateAndDrawParticles();

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
```