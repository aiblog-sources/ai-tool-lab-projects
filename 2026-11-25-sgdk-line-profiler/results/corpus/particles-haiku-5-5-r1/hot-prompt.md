Here is a Mega Drive game written with SGDK 2.09. Make it run faster on the real hardware (68000 at 7.67 MHz) without changing what the game does or how it looks. Reply with the full main.c in one ```c code block.

Below is a line profile of this code, measured in an emulator with a scripted pad: cycles per video frame by source line (one NTSC frame is 128,005 cycles), what each line called, and the findings of a performance lint on the same lines.

== Lines by measured cost (sgdkhot: mdlines + sgdklint) ==
budget 128,005 cycles per video frame (NTSC); 1,200 frames counted; busy 17,167 cycles per frame on average (13.4%)
each line = its own instructions + every library/libgcc call made from it (game helpers count on their own lines)
    4,472 /frame  26.1% of busy  src/main.c:147  SYS_doVBlankProcess();
                     4,444 in SYS_doVBlankProcessEx.constprop.0.isra.0: 1.00 calls/frame, 4,444 cycles each
    2,755 /frame  16.0% of busy  src/main.c:96  VDP_setSprite(i, px, py, SPRITE_SIZE(1, 1), attr);
    2,554 /frame  14.9% of busy  src/main.c:133  JOY_update();
                     2,536 in JOY_update: 1.00 calls/frame, 2,536 cycles each
    1,564 /frame   9.1% of busy  src/main.c:73  for (u16 i = 0; i < PARTICLE_COUNT; i++)
    1,041 /frame   6.1% of busy  src/main.c:77  if (p->alive)
      976 /frame   5.7% of busy  src/main.c:146  VDP_updateSprites(PARTICLE_COUNT, DMA_QUEUE);
                       794 in DMA_queueDmaFast: 1.00 calls/frame, 794 cycles each
      774 /frame   4.5% of busy  src/main.c:87  p->y  += p->vy;
      604 /frame   3.5% of busy  src/main.c:86  p->x  += p->vx;
      413 /frame   2.4% of busy  src/main.c:79  p->life--;
      349 /frame   2.0% of busy  src/main.c:88  p->vy += GRAVITY;
      317 /frame   1.8% of busy  [no game line]:0  
                       126 in no_bmp_task
                       123 in no_user_task
                        31 in no_xgm_task
      294 /frame   1.7% of busy  src/main.c:101  VDP_setSprite(i, 0, -16, SPRITE_SIZE(1, 1), attr);
      236 /frame   1.4% of busy  src/main.c:94  s16 px = (s16)(p->x >> 4);
      227 /frame   1.3% of busy  src/main.c:95  s16 py = (s16)(p->y >> 4);
      144 /frame   0.8% of busy  src/main.c:80  if (p->life == 0)

== Lint findings, by measured cost ==
negligible        89 /frame (0.07% of a frame)  src/main.c:62 [mul32] guarded
negligible        88 /frame (0.07% of a frame)  src/main.c:63 [mul32] guarded
verdicts: matters >= 1% of a frame, small >= 0.2%, negligible below, not run = no cycles on that line in this run

== Game lines over 2% of a frame with no lint finding ==
    4,472 /frame  src/main.c:147 (main), mostly SYS_doVBlankProcessEx.constprop.0.isra.0
    2,755 /frame  src/main.c:96 (updateParticles), mostly own code

library code with no game line on the stack (interrupt handlers, SGDK's VBlank work): 317 cycles per frame

main.c:
#include <genesis.h>

#define PARTICLE_COUNT      48
#define EXPLOSION_FRAMES    60      /* ~1 second at 60 Hz (NTSC) */
#define GRAVITY             2       /* 1/16 px per frame^2 */
#define SCREEN_W            320
#define SCREEN_H            224
#define CENTER_X            160
#define CENTER_Y            112

/* 8x8 4bpp particle tile, colour index 1 (8 rows x 8 pixels, 1 nibble each) */
static const u32 particleTile[8] =
{
    0x00111100,
    0x01111110,
    0x11111111,
    0x11111111,
    0x11111111,
    0x11111111,
    0x01111110,
    0x00111100
};

/* cos/sin of 0, 7.5, 15, ... 82.5 degrees, scaled by 64 */
static const s16 cosT[12] = { 64, 63, 62, 59, 55, 51, 45, 39, 32, 24, 17,  8 };
static const s16 sinT[12] = {  0,  8, 17, 24, 32, 39, 45, 51, 55, 59, 62, 63 };

typedef struct
{
    s32 x, y;       /* position in 1/16 px */
    s32 vx, vy;     /* velocity in 1/16 px per frame */
    u16 life;       /* frames remaining */
    u8  alive;
} Particle;

static Particle particles[PARTICLE_COUNT];

static void spawnExplosion(s32 cx, s32 cy)
{
    for (u16 i = 0; i < PARTICLE_COUNT; i++)
    {
        u16 q = i / 12;             /* quadrant 0..3 */
        u16 r = i % 12;             /* angle step within quadrant */
        s32 c = cosT[r];
        s32 s = sinT[r];
        s32 dx, dy;

        /* Screen coordinates: y grows downward */
        switch (q)
        {
            case 0:  dx =  c; dy = -s; break;
            case 1:  dx = -s; dy = -c; break;
            case 2:  dx = -c; dy =  s; break;
            default: dx =  s; dy =  c; break;
        }

        /* Random speed: 2.5 to 5 px per frame */
        s32 spd = 40 + (s32)(random() % 41);

        particles[i].x     = cx << 4;
        particles[i].y     = cy << 4;
        particles[i].vx    = (dx * spd) / 64;
        particles[i].vy    = (dy * spd) / 64;
        particles[i].life  = EXPLOSION_FRAMES;
        particles[i].alive = 1;
    }
}

static void updateParticles(void)
{
    u16 attr = TILE_ATTR_FULL(PAL1, 0, 0, 0, TILE_USER_INDEX);

    for (u16 i = 0; i < PARTICLE_COUNT; i++)
    {
        Particle *p = &particles[i];

        if (p->alive)
        {
            p->life--;
            if (p->life == 0)
            {
                p->alive = 0;
            }
            else
            {
                p->x  += p->vx;
                p->y  += p->vy;
                p->vy += GRAVITY;
            }
        }

        if (p->alive)
        {
            s16 px = (s16)(p->x >> 4);
            s16 py = (s16)(p->y >> 4);
            VDP_setSprite(i, px, py, SPRITE_SIZE(1, 1), attr);
        }
        else
        {
            /* Park off-screen (above the visible area) */
            VDP_setSprite(i, 0, -16, SPRITE_SIZE(1, 1), attr);
        }
    }
}

int main(bool hardReset)
{
    (void)hardReset;

    JOY_init();
    VDP_setScreenWidth320();

    /* Palettes: text (PAL0) and particles (PAL1) */
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x000000));   /* background */
    PAL_setColor(15, RGB24_TO_VDPCOLOR(0xFFFFFF));  /* text */
    PAL_setColor(17, RGB24_TO_VDPCOLOR(0xFF8000));  /* particle (PAL1 idx 1) */

    VDP_setTextPalette(PAL0);
    VDP_drawText("PRESS A TO EXPLODE", 11, 1);

    /* Load particle tile into VRAM (no resource files) */
    VDP_loadTileData(particleTile, TILE_USER_INDEX, 1, 0);

    for (u16 i = 0; i < PARTICLE_COUNT; i++)
    {
        particles[i].alive = 0;
    }

    u16 prevButtons = 0;

    while (TRUE)
    {
        JOY_update();

        u16 buttons = JOY_readJoypad(JOY_1);

        /* Edge detection: trigger only on the frame A goes down */
        if ((buttons & BUTTON_A) && !(prevButtons & BUTTON_A))
        {
            spawnExplosion(CENTER_X, CENTER_Y);
        }

        prevButtons = buttons;

        updateParticles();
        VDP_updateSprites(PARTICLE_COUNT, DMA_QUEUE);
        SYS_doVBlankProcess();
    }

    return 0;
}
