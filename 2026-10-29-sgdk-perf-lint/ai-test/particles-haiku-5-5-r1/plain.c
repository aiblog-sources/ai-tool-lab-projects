#include <genesis.h>

#define PARTICLE_COUNT      48
#define EXPLOSION_FRAMES    60      /* ~1 second at 60 Hz (NTSC) */
#define GRAVITY             2       /* 1/16 px per frame^2 */
#define CENTER_X            160
#define CENTER_Y            112
#define PARK_Y              (-16)   /* above the visible area */

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

/*
 * Positions and velocities in 1/16 px. All values stay within s16:
 * |vx|, |vy| <= 80 at spawn (plus gravity), and |y| <= ~10200 over the
 * 60-frame lifetime, so 16-bit storage gives identical results.
 */
typedef struct
{
    s16 x, y;
    s16 vx, vy;
    u16 life;       /* frames remaining; 0 = dead (sprite already parked) */
} Particle;

static Particle particles[PARTICLE_COUNT];
static u16 spritesDirty;
static u16 spriteAttr;

/* Signed division by 64 truncating toward zero (same as C's /64) without DIVS */
static s16 divTrunc64(s16 v)
{
    return (s16)((v + ((v >> 15) & 63)) >> 6);
}

static void spawnExplosion(s16 cx, s16 cy)
{
    u16 i = 0;

    for (u16 q = 0; q < 4; q++)             /* quadrant 0..3 */
    {
        for (u16 r = 0; r < 12; r++, i++)   /* angle step within quadrant */
        {
            s16 c = cosT[r];
            s16 s = sinT[r];
            s16 dx, dy;

            /* Screen coordinates: y grows downward */
            switch (q)
            {
                case 0:  dx =  c; dy = -s; break;
                case 1:  dx = -s; dy = -c; break;
                case 2:  dx = -c; dy =  s; break;
                default: dx =  s; dy =  c; break;
            }

            /* Random speed: 2.5 to 5 px per frame */
            s16 spd = 40 + (s16)(random() % 41);

            Particle *p = &particles[i];
            p->x    = (s16)(cx << 4);
            p->y    = (s16)(cy << 4);
            p->vx   = divTrunc64((s16)(dx * spd));
            p->vy   = divTrunc64((s16)(dy * spd));
            p->life = EXPLOSION_FRAMES;
        }
    }
}

static void updateParticles(void)
{
    for (u16 i = 0; i < PARTICLE_COUNT; i++)
    {
        Particle *p = &particles[i];

        if (p->life == 0)
        {
            continue;   /* dead and already parked in the sprite cache */
        }

        spritesDirty = 1;

        if (--p->life == 0)
        {
            /* Park off-screen (above the visible area) */
            VDP_setSprite(i, 0, PARK_Y, SPRITE_SIZE(1, 1), spriteAttr);
        }
        else
        {
            p->x  += p->vx;
            p->y  += p->vy;
            p->vy += GRAVITY;

            VDP_setSprite(i, (s16)(p->x >> 4), (s16)(p->y >> 4),
                          SPRITE_SIZE(1, 1), spriteAttr);
        }
    }
}

static void initParticles(void)
{
    for (u16 i = 0; i < PARTICLE_COUNT; i++)
    {
        particles[i].life = 0;
        VDP_setSprite(i, 0, PARK_Y, SPRITE_SIZE(1, 1), spriteAttr);
    }
    spritesDirty = 1;
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

    spriteAttr = TILE_ATTR_FULL(PAL1, 0, 0, 0, TILE_USER_INDEX);
    initParticles();

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

        /* Only push the sprite table to VRAM when something changed */
        if (spritesDirty)
        {
            VDP_updateSprites(PARTICLE_COUNT, DMA_QUEUE);
            spritesDirty = 0;
        }

        SYS_doVBlankProcess();
    }

    return 0;
}
