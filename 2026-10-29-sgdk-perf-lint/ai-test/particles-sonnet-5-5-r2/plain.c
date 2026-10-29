/*
 * main.c - SGDK 2.09
 * Press A on pad 1: an explosion of 48 particles (hardware sprites) flies out in
 * all directions, is pulled down by gravity, and disappears after 1 second.
 * No resource files: the tile is a const u32 array loaded with VDP_loadTileData.
 */

#include <genesis.h>

#define EXPLOSION_PARTICLES   48
#define MAX_PARTICLES         80      /* hardware limit: 80 sprites per frame */
#define FRAC_BITS             8       /* positions/velocities are 8.8 fixed point */
#define GRAVITY               26      /* ~0.10 px/frame^2 in 8.8 */

#define PARTICLE_TILE_INDEX   (TILE_USER_INDEX)

/* 8x8 4bpp tile: small glowing ball (1 = core, 2 = edge) */
static const u32 particleTile[8] =
{
    0x00022000,
    0x02211220,
    0x02111120,
    0x21111112,
    0x21111112,
    0x02111120,
    0x02211220,
    0x00022000
};

/* Three palettes (index 0 transparent, 1 = core, 2 = edge), format 0BGR */
static const u16 palFire[16]  = { 0x0000, 0x0EEE, 0x00AE, 0,0,0,0,0,0,0,0,0,0,0,0,0 };
static const u16 palRed[16]   = { 0x0000, 0x08EE, 0x002E, 0,0,0,0,0,0,0,0,0,0,0,0,0 };
static const u16 palBlue[16]  = { 0x0000, 0x0EEE, 0x0E84, 0,0,0,0,0,0,0,0,0,0,0,0,0 };

typedef struct
{
    s32 x, y;        /* 8.8 fixed point, screen coordinates */
    s16 vx, vy;      /* 8.8 fixed point, pixels per frame */
    u16 life;        /* frames remaining, 0 = free slot */
    u16 attr;        /* precomputed sprite attribute word (palette + tile) */
} Particle;

static Particle particles[MAX_PARTICLES];
static u16 lifeFrames;
static u16 blinkThreshold;
static u16 palAttr[3];

static void spawnExplosion(void)
{
    s32 cx = (s32) (64 + ((u16) random() % 192)) << FRAC_BITS;
    s32 cy = (s32) (60 + ((u16) random() % 100)) << FRAC_BITS;
    u16 toSpawn = EXPLOSION_PARTICLES;
    Particle *p = particles;
    Particle *end = particles + MAX_PARTICLES;

    for (; p < end && toSpawn; p++)
    {
        if (p->life) continue;

        /* random direction (10-bit angle) and speed 1.0 .. 4.0 px/frame */
        u16 angle = (u16) random() & 1023;
        s16 speed = (s16) (256 + ((u16) random() % 768));

        /* 16x16 -> 32 bit multiplies (muls.w) instead of 32x32 library calls */
        p->x  = cx;
        p->y  = cy;
        p->vx = (s16) (((s32) (s16) cosFix16(angle) * speed) >> 10);
        p->vy = (s16) (((s32) (s16) sinFix16(angle) * speed) >> 10);
        p->life = lifeFrames;
        p->attr = palAttr[(u16) random() % 3];
        toSpawn--;
    }
}

static void updateAndDraw(void)
{
    u16 n = 0;
    VDPSprite *spr = vdpSpriteCache;
    Particle *p = particles;
    Particle *end = particles + MAX_PARTICLES;
    const u16 thr = blinkThreshold;

    for (; p < end; p++)
    {
        u16 life = p->life;
        if (!life) continue;

        life--;
        p->life = life;

        s16 vy = p->vy + GRAVITY;
        p->vy = vy;
        s32 x = p->x + p->vx;
        s32 y = p->y + vy;
        p->x = x;
        p->y = y;

        if (!life) continue;

        /* blink during the last quarter of the life to fade out */
        if (life < thr && (life & 2)) continue;

        s16 sx = (s16) (x >> FRAC_BITS) - 4;
        s16 sy = (s16) (y >> FRAC_BITS) - 4;

        /* skip particles fully outside the 320x224 screen */
        if ((u16) (sx + 8) > 328 || (u16) (sy + 8) > 232) continue;

        n++;
        spr->y = sy + 0x80;
        spr->size_link = (SPRITE_SIZE(1, 1) << 8) | n;
        spr->attribut = p->attr;
        spr->x = sx + 0x80;
        spr++;
    }

    if (n == 0)
    {
        /* keep one sprite parked off-screen so the list stays valid */
        VDP_setSpriteFull(0, -32, -32, SPRITE_SIZE(1, 1),
                          TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, PARTICLE_TILE_INDEX), 0);
        n = 1;
    }
    else
    {
        /* terminate the link list */
        VDP_setSpriteLink(n - 1, 0);
    }

    VDP_updateSprites(n, DMA);
}

int main(bool hardReset)
{
    u16 prev = 0;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    lifeFrames = IS_PAL_SYSTEM ? 50 : 60;   /* exactly 1 second */
    blinkThreshold = lifeFrames >> 2;

    palAttr[0] = TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, PARTICLE_TILE_INDEX);
    palAttr[1] = TILE_ATTR_FULL(PAL2, TRUE, FALSE, FALSE, PARTICLE_TILE_INDEX);
    palAttr[2] = TILE_ATTR_FULL(PAL3, TRUE, FALSE, FALSE, PARTICLE_TILE_INDEX);

    JOY_init();

    VDP_loadTileData(particleTile, PARTICLE_TILE_INDEX, 1, CPU);
    PAL_setPalette(PAL1, palFire, CPU);
    PAL_setPalette(PAL2, palRed, CPU);
    PAL_setPalette(PAL3, palBlue, CPU);

    VDP_resetSprites();

    VDP_drawText("Press A to explode", 11, 1);

    for (u16 i = 0; i < MAX_PARTICLES; i++) particles[i].life = 0;

    setRandomSeed(1234);

    while (TRUE)
    {
        u16 pad = JOY_readJoypad(JOY_1);

        if ((pad & BUTTON_A) && !(prev & BUTTON_A))
        {
            setRandomSeed((u16) (getTick() | 1));
            spawnExplosion();
        }
        prev = pad;

        updateAndDraw();
        SYS_doVBlankProcess();
    }

    return 0;
}
