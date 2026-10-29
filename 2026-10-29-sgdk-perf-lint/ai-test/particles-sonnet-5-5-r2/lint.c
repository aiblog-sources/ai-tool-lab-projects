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
    u8  pal;         /* PAL1..PAL3 */
} Particle;

static Particle particles[MAX_PARTICLES];
static u16 lifeFrames;

/* u16 % u16 so the compiler can use a single DIVU.W (117 cycles) */
static inline u16 randMod(u16 n)
{
    u16 r = random();
    return (u16) (r % n);
}

static void spawnExplosion(void)
{
    u16 rx = randMod(192);
    u16 ry = randMod(100);
    s32 cx = (s32) (64 + rx) << FRAC_BITS;
    s32 cy = (s32) (60 + ry) << FRAC_BITS;
    u16 toSpawn = EXPLOSION_PARTICLES;

    for (u16 i = 0; i < MAX_PARTICLES && toSpawn; i++)
    {
        Particle *p = &particles[i];
        if (p->life) continue;

        /* random direction (10-bit angle) and speed 1.0 .. 4.0 px/frame */
        u16 angle = random() & 1023;
        u16 rs = randMod(768);
        s32 speed = 256 + rs;
        u16 rp = randMod(3);

        p->x  = cx;
        p->y  = cy;
        p->vx = (s16) ((cosFix16(angle) * speed) >> 10);
        p->vy = (s16) ((sinFix16(angle) * speed) >> 10);
        p->life = lifeFrames;
        p->pal  = PAL1 + rp;
        toSpawn--;
    }
}

static void updateAndDraw(void)
{
    u16 n = 0;

    for (u16 i = 0; i < MAX_PARTICLES; i++)
    {
        Particle *p = &particles[i];
        if (!p->life) continue;

        p->life--;
        p->vy += GRAVITY;
        p->x  += p->vx;
        p->y  += p->vy;

        if (!p->life) continue;

        /* blink during the last quarter of the life to fade out */
        if (p->life < (lifeFrames >> 2) && (p->life & 2)) continue;

        s16 sx = (s16) (p->x >> FRAC_BITS) - 4;
        s16 sy = (s16) (p->y >> FRAC_BITS) - 4;

        /* skip particles fully outside the 320x224 screen */
        if (sx < -8 || sx > 320 || sy < -8 || sy > 224) continue;

        VDP_setSpriteFull(n, sx, sy, SPRITE_SIZE(1, 1),
                          TILE_ATTR_FULL(p->pal, TRUE, FALSE, FALSE, PARTICLE_TILE_INDEX),
                          n + 1);
        n++;
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
