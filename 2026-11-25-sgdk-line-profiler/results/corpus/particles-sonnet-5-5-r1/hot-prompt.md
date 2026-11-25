Here is a Mega Drive game written with SGDK 2.09. Make it run faster on the real hardware (68000 at 7.67 MHz) without changing what the game does or how it looks. Reply with the full main.c in one ```c code block.

Below is a line profile of this code, measured in an emulator with a scripted pad: cycles per video frame by source line (one NTSC frame is 128,005 cycles), what each line called, and the findings of a performance lint on the same lines.

== Lines by measured cost (sgdkhot: mdlines + sgdklint) ==
budget 128,005 cycles per video frame (NTSC); 1,200 frames counted; busy 16,201 cycles per frame on average (12.7%)
each line = its own instructions + every library/libgcc call made from it (game helpers count on their own lines)
    4,466 /frame  27.6% of busy  src/main.c:158  SYS_doVBlankProcess();
                     4,438 in SYS_doVBlankProcessEx.constprop.0.isra.0: 1.00 calls/frame, 4,456 cycles each
    3,868 /frame  23.9% of busy  src/main.c:49  VDP_setSpriteFull(i, 0, 0, SPRITE_SIZE(1, 1),
    1,899 /frame  11.7% of busy  src/main.c:78  for (u16 i = 0; i < NUM_PARTICLES; i++)
    1,088 /frame   6.7% of busy  src/main.c:82  if (p->life == 0)
      989 /frame   6.1% of busy  src/main.c:156  VDP_updateSprites(NUM_PARTICLES, DMA_QUEUE);
                       792 in DMA_queueDmaFast: 1.00 calls/frame, 796 cycles each
      586 /frame   3.6% of busy  src/main.c:98  bool offscreen = (sx < -8) || (sx > 320) || (sy < -8) || (sy
      570 /frame   3.5% of busy  src/main.c:106  VDP_setSpriteFull(i,
      342 /frame   2.1% of busy  src/main.c:89  p->x += p->vx;
      341 /frame   2.1% of busy  src/main.c:88  p->vy += GRAVITY;
      317 /frame   2.0% of busy  [no game line]:0  
                       125 in no_bmp_task
                       122 in no_user_task
                        31 in no_xgm_task
      292 /frame   1.8% of busy  src/main.c:90  p->y += p->vy;
      287 /frame   1.8% of busy  src/main.c:97  bool blinkHidden = (p->life < (lifeFrames >> 2)) && (p->life
      232 /frame   1.4% of busy  src/main.c:66  fix16 speed = FIX16(0.8) + (fix16)(random() % FIX16(2.7));
                       200 in __modsi3: 0.20 calls/frame, 1,000 cycles each
                 lint [div] guarded: '%' by a FIX16()/FIX32() constant: the macro casts to a signed type, so this is a signed 32-bit %: 1,062 cycle
      218 /frame   1.3% of busy  src/main.c:93  s16 sx = F16_toInt(p->x);
      195 /frame   1.2% of busy  src/main.c:91  p->life--;

== Lint findings, by measured cost ==
negligible       232 /frame (0.18% of a frame)  src/main.c:66 [div] guarded
verdicts: matters >= 1% of a frame, small >= 0.2%, negligible below, not run = no cycles on that line in this run

== Game lines over 2% of a frame with no lint finding ==
    4,466 /frame  src/main.c:158 (main), mostly SYS_doVBlankProcessEx.constprop.0.isra.0
    3,868 /frame  src/main.c:49 (hideSprite), mostly own code

library code with no game line on the stack (interrupt handlers, SGDK's VBlank work): 317 cycles per frame

main.c:
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
    /* random origin in the middle area of the 320x224 screen */
    fix16 cx = FIX16(40 + (random() % 240));
    fix16 cy = FIX16(50 + (random() % 90));

    for (u16 i = 0; i < NUM_PARTICLES; i++)
    {
        Particle *p = &particles[i];
        if (p->life) continue;              /* only reuse free particles */

        u16 angle = random() & 1023;        /* 0..1023 = full circle */
        fix16 speed = FIX16(0.8) + (fix16)(random() % FIX16(2.7));

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
