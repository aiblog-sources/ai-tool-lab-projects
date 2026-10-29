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
