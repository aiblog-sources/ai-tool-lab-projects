Here is a Mega Drive game written with SGDK 2.09. Make it run faster on the real hardware (68000 at 7.67 MHz) without changing what the game does or how it looks. Reply with the full main.c in one ```c code block.

Below is a line profile of this code, measured in an emulator with a scripted pad: cycles per video frame by source line (one NTSC frame is 128,005 cycles), what each line called, and the findings of a performance lint on the same lines.

== Lines by measured cost (sgdkhot: mdlines + sgdklint) ==
budget 128,005 cycles per video frame (NTSC); 1,200 frames counted; busy 16,428 cycles per frame on average (12.8%)
each line = its own instructions + every library/libgcc call made from it (game helpers count on their own lines)
    3,794 /frame  23.1% of busy  src/main.c:233  SYS_doVBlankProcess();
                     3,766 in SYS_doVBlankProcessEx.constprop.0.isra.0: 1.00 calls/frame, 3,766 cycles each
    1,313 /frame   8.0% of busy  src/main.c:156  VDP_setSpriteFull(count,
    1,246 /frame   7.6% of busy  src/main.c:149  if (p->life <= 0) continue;
    1,162 /frame   7.1% of busy  src/main.c:123  for (u16 i = 0; i < NUM_PARTICLES; i++)
    1,162 /frame   7.1% of busy  src/main.c:146  for (u16 i = 0; i < NUM_PARTICLES; i++)
    1,044 /frame   6.4% of busy  src/main.c:126  if (p->life <= 0) continue;
    1,035 /frame   6.3% of busy  src/main.c:178  VDP_updateSprites(count, DMA_QUEUE);
                       794 in DMA_queueDmaFast: 1.00 calls/frame, 794 cycles each
      522 /frame   3.2% of busy  src/main.c:129  p->x  += p->vx;
      474 /frame   2.9% of busy  src/main.c:158  FROM_FP(p->y) - 4,
      458 /frame   2.8% of busy  src/main.c:128  p->vy += GRAVITY;
      435 /frame   2.7% of busy  src/main.c:136  if (sx < -8 || sx > SCREEN_W + 8 || sy > SCREEN_H + 8)
      427 /frame   2.6% of busy  src/main.c:157  FROM_FP(p->x) - 4,
      409 /frame   2.5% of busy  src/main.c:130  p->y  += p->vy;
      326 /frame   2.0% of busy  [no game line]:0  
                       124 in no_bmp_task
                       124 in no_user_task
                        30 in no_xgm_task
      309 /frame   1.9% of busy  src/main.c:152  if (p->life > third * 2)      tile = 0;

== Lint findings, by measured cost ==
negligible       144 /frame (0.11% of a frame)  src/main.c:144 [div] frame
negligible        82 /frame (0.06% of a frame)  src/main.c:115 [mul32] guarded
negligible        78 /frame (0.06% of a frame)  src/main.c:114 [mul32] guarded
negligible        30 /frame (0.02% of a frame)  src/main.c:110 [div] guarded
verdicts: matters >= 1% of a frame, small >= 0.2%, negligible below, not run = no cycles on that line in this run

== Game lines over 2% of a frame with no lint finding ==
    3,794 /frame  src/main.c:233 (main), mostly SYS_doVBlankProcessEx.constprop.0.isra.0

library code with no game line on the stack (interrupt handlers, SGDK's VBlank work): 326 cycles per frame

main.c:
/*
 * main.c - Particle explosion demo for SGDK 2.09 (Sega Mega Drive / Genesis)
 *
 * Press A on pad 1 to spawn an explosion of 48 particle sprites.
 * The particles fly out in all directions, fall under gravity, shrink
 * as they age, and disappear after 1 second.
 *
 * No resource files are used. The tiles are const u32 arrays loaded
 * with VDP_loadTileData. The hardware sprite table is written directly
 * with VDP_setSpriteFull / VDP_updateSprites.
 */

#include <genesis.h>

#define NUM_PARTICLES   48

/* Fixed point: 8 fractional bits */
#define FP_SHIFT        8
#define TO_FP(x)        ((s32)(x) << FP_SHIFT)
#define FROM_FP(x)      ((s16)((x) >> FP_SHIFT))

#define GRAVITY         28      /* ~0.11 px/frame^2 */
#define UP_BIAS         200     /* initial upward push, ~0.78 px/frame */
#define SPEED_MIN       384     /* 1.5 px/frame */
#define SPEED_RANGE     512     /* +0..2 px/frame */

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
    s32 x, y;       /* position, fixed point */
    s32 vx, vy;     /* velocity, fixed point */
    s16 life;       /* frames remaining, 0 = dead */
} Particle;

static Particle particles[NUM_PARTICLES];
static s16 lifeFrames;

static void spawnExplosion(s16 cx, s16 cy)
{
    for (u16 i = 0; i < NUM_PARTICLES; i++)
    {
        Particle *p = &particles[i];
        s32 speed = SPEED_MIN + (random() % SPEED_RANGE);

        p->x = TO_FP(cx);
        p->y = TO_FP(cy);
        p->vx = ((s32)cos48(i) * speed) >> 8;
        p->vy = (((s32)sin48(i) * speed) >> 8) - UP_BIAS;
        /* Small random lifetime spread so the explosion doesn't vanish in one frame */
        p->life = lifeFrames - (random() & 3);
    }
}

static void updateParticles(void)
{
    for (u16 i = 0; i < NUM_PARTICLES; i++)
    {
        Particle *p = &particles[i];
        if (p->life <= 0) continue;

        p->vy += GRAVITY;
        p->x  += p->vx;
        p->y  += p->vy;
        p->life--;

        /* Kill particles that left the screen */
        s16 sx = FROM_FP(p->x);
        s16 sy = FROM_FP(p->y);
        if (sx < -8 || sx > SCREEN_W + 8 || sy > SCREEN_H + 8)
            p->life = 0;
    }
}

static void drawParticles(void)
{
    u16 count = 0;
    s16 third = lifeFrames / 3;

    for (u16 i = 0; i < NUM_PARTICLES; i++)
    {
        Particle *p = &particles[i];
        if (p->life <= 0) continue;

        u16 tile;
        if (p->life > third * 2)      tile = 0;
        else if (p->life > third)     tile = 1;
        else                          tile = 2;

        VDP_setSpriteFull(count,
                          FROM_FP(p->x) - 4,
                          FROM_FP(p->y) - 4,
                          SPRITE_SIZE(1, 1),
                          TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, PARTICLE_TILE_BASE + tile),
                          count + 1);
        count++;
    }

    if (count == 0)
    {
        /* Keep one sprite in the table, parked off-screen */
        VDP_setSpriteFull(0, -32, -32, SPRITE_SIZE(1, 1),
                          TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, PARTICLE_TILE_BASE), 0);
        count = 1;
    }
    else
    {
        /* Terminate the link chain */
        VDP_setSpriteLink(count - 1, 0);
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

    VDP_resetSprites();
    drawParticles();

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

        updateParticles();
        drawParticles();

        SYS_doVBlankProcess();
    }

    return 0;
}
