/*
 * main.c - SGDK 2.09
 * Press A: explosion of 48 sprite particles with gravity, lasting 1 second.
 * No resource files: the tile and palette are defined in code.
 *
 * Optimisations (behaviour and visuals unchanged):
 *  - Sprite size, link and attributes never change after init, so the per-frame
 *    loop only writes x/y directly into the sprite cache instead of calling
 *    VDP_setSpriteFull() (a full function call with 5 stores) per particle.
 *  - Off-screen test uses one unsigned compare per axis.
 *  - Modulo operations use 16-bit operands (DIVU) instead of the 32-bit
 *    library routine that int promotion would pull in.
 *  - Loop-invariant values are hoisted; pointers are walked instead of indexed.
 */

#include <genesis.h>

#define NUM_PARTICLES   48
#define PARTICLE_TILE   TILE_USER_INDEX
#define SPR_OFFSET      128                 /* VDP sprite coordinate offset */

#define GRAVITY         FIX16(0.12)         /* per frame */

typedef struct
{
    fix16 x, y;
    fix16 vx, vy;
    u16   life;                             /* frames left, 0 = inactive */
} Particle;

static Particle particles[NUM_PARTICLES];
static u16 lifeFrames;                      /* frames in 1 second (50 PAL / 60 NTSC) */

/* 8x8 round particle, 4bpp (one nibble per pixel, colour indexes 0..3) */
static const u32 particleTile[8] =
{
    0x00111100,
    0x01122110,
    0x11233211,
    0x12333321,
    0x12333321,
    0x11233211,
    0x01122110,
    0x00111100
};

static const u16 particlePalette[16] =
{
    0x0000, 0x006C, 0x02AE, 0x0CEE,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
};

/* one-time sprite setup: size, link and attributes stay constant afterwards */
static void initSprite(u16 i)
{
    /* y = 0 (raw) is above the visible area, so the sprite is hidden */
    VDP_setSpriteFull(i, 0, 0, SPRITE_SIZE(1, 1),
                      TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, PARTICLE_TILE),
                      (i == NUM_PARTICLES - 1) ? 0 : (i + 1));
}

static void spawnExplosion(void)
{
    /* random origin in the middle area of the 320x224 screen */
    fix16 cx = FIX16(40 + (u16)((u16)random() % (u16)240));
    fix16 cy = FIX16(50 + (u16)((u16)random() % (u16)90));

    Particle *p = particles;
    for (u16 i = 0; i < NUM_PARTICLES; i++, p++)
    {
        if (p->life) continue;              /* only reuse free particles */

        u16 angle = random() & 1023;        /* 0..1023 = full circle */
        fix16 speed = FIX16(0.8) + (fix16)((u16)random() % (u16)FIX16(2.7));

        p->x = cx;
        p->y = cy;
        p->vx = F16_mul(cosFix16(angle), speed);
        p->vy = F16_mul(sinFix16(angle), speed) - FIX16(1.0); /* slight upward kick */
        p->life = lifeFrames;
    }
}

static void updateParticles(void)
{
    Particle  *p   = particles;
    VDPSprite *spr = vdpSpriteCache;
    const u16 blinkLimit = lifeFrames >> 2;

    for (u16 i = 0; i < NUM_PARTICLES; i++, p++, spr++)
    {
        u16 life = p->life;

        if (life == 0)
        {
            spr->y = 0;                     /* hidden */
            spr->x = 0;
            continue;
        }

        fix16 vy = p->vy + GRAVITY;
        fix16 x  = p->x + p->vx;
        fix16 y  = p->y + vy;

        p->vy = vy;
        p->x  = x;
        p->y  = y;
        life--;
        p->life = life;

        s16 sx = x >> FIX16_FRAC_BITS;
        s16 sy = y >> FIX16_FRAC_BITS;

        /* blink during the last quarter of life, hide when off-screen */
        if (((life < blinkLimit) && (life & 2)) ||
            ((u16)(sx + 8) > 328) ||        /* sx < -8 || sx > 320 */
            ((u16)(sy + 8) > 232))          /* sy < -8 || sy > 224 */
        {
            spr->y = 0;
            spr->x = 0;
        }
        else
        {
            spr->x = sx - 4 + SPR_OFFSET;
            spr->y = sy - 4 + SPR_OFFSET;
        }
    }
}

int main(bool hardReset)
{
    (void)hardReset;

    VDP_setScreenWidth320();
    JOY_init();

    lifeFrames = IS_PAL_SYSTEM ? 50 : 60;

    /* graphics: tile + palette defined in code */
    VDP_loadTileData(particleTile, PARTICLE_TILE, 1, DMA);
    PAL_setPalette(PAL1, particlePalette, DMA);

    VDP_drawText("Press A to explode", 11, 1);

    /* initialise sprite list: all particles hidden */
    VDP_resetSprites();
    for (u16 i = 0; i < NUM_PARTICLES; i++)
    {
        particles[i].life = 0;
        initSprite(i);
    }
    VDP_updateSprites(NUM_PARTICLES, DMA_QUEUE);

    setRandomSeed(0x1234);

    u16 prevPad = 0;

    while (TRUE)
    {
        u16 pad = JOY_readJoypad(JOY_1);

        if ((pad & BUTTON_A) && !(prevPad & BUTTON_A))
        {
            setRandomSeed(getTick() ^ 0x5A5A);   /* vary sequence with timing */
            spawnExplosion();
        }
        prevPad = pad;

        updateParticles();
        VDP_updateSprites(NUM_PARTICLES, DMA_QUEUE);

        SYS_doVBlankProcess();
    }

    return 0;
}
