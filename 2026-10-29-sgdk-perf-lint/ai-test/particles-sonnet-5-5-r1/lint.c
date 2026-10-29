/*
 * main.c - SGDK 2.09
 * Press A: explosion of 48 sprite particles with gravity, lasting 1 second.
 * No resource files: the tile and palette are defined in code.
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

static void hideSprite(u16 i)
{
    /* y = 0 (raw) is above the visible area, so the sprite is hidden */
    VDP_setSpriteFull(i, 0, 0, SPRITE_SIZE(1, 1),
                      TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, PARTICLE_TILE),
                      (i == NUM_PARTICLES - 1) ? 0 : (i + 1));
}

static void spawnExplosion(void)
{
    /* all modulo operands are u16 so the compiler uses the 16-bit divu */
    const u16 rangeX = 240;
    const u16 rangeY = 90;
    const u16 rangeSpeed = (u16)FIX16(2.7);

    u16 rx = random();
    u16 ry = random();

    /* random origin in the middle area of the 320x224 screen */
    u16 ox = rx % rangeX;
    u16 oy = ry % rangeY;
    fix16 cx = (fix16)((u16)(40 + ox) << FIX16_FRAC_BITS);
    fix16 cy = (fix16)((u16)(50 + oy) << FIX16_FRAC_BITS);

    for (u16 i = 0; i < NUM_PARTICLES; i++)
    {
        Particle *p = &particles[i];
        if (p->life) continue;              /* only reuse free particles */

        u16 angle = random() & 1023;        /* 0..1023 = full circle */
        u16 rs = random();
        u16 rem = rs % rangeSpeed;
        fix16 speed = FIX16(0.8) + (fix16)rem;

        p->x = cx;
        p->y = cy;
        p->vx = F16_mul(cosFix16(angle), speed);
        p->vy = F16_mul(sinFix16(angle), speed) - FIX16(1.0); /* slight upward kick */
        p->life = lifeFrames;
    }
}

static void updateParticles(void)
{
    for (u16 i = 0; i < NUM_PARTICLES; i++)
    {
        Particle *p = &particles[i];

        if (p->life == 0)
        {
            hideSprite(i);
            continue;
        }

        p->vy += GRAVITY;
        p->x += p->vx;
        p->y += p->vy;
        p->life--;

        s16 sx = F16_toInt(p->x);
        s16 sy = F16_toInt(p->y);

        /* blink during the last quarter of life, hide when off-screen */
        bool blinkHidden = (p->life < (lifeFrames >> 2)) && (p->life & 2);
        bool offscreen = (sx < -8) || (sx > 320) || (sy < -8) || (sy > 224);

        if (blinkHidden || offscreen)
        {
            hideSprite(i);
        }
        else
        {
            VDP_setSpriteFull(i,
                              sx - 4 + SPR_OFFSET,
                              sy - 4 + SPR_OFFSET,
                              SPRITE_SIZE(1, 1),
                              TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, PARTICLE_TILE),
                              (i == NUM_PARTICLES - 1) ? 0 : (i + 1));
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
        hideSprite(i);
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
