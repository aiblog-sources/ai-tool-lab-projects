#include <genesis.h>

/* ------------------------------------------------------------------------ */
/* Configuration                                                            */
/* ------------------------------------------------------------------------ */
#define NUM_BULLETS        16
#define BOSS_ANIM_FRAMES   8
#define BOSS_ANIM_DELAY    4          /* video frames per animation frame   */

#define BOSS_CELL_X        12         /* 12*8 = 96 px  -> centred on 320    */
#define BOSS_CELL_Y        2          /* 2*8  = 16 px                       */
#define BOSS_CELLS         16         /* 16*8 = 128 px                      */
#define CORE_CELL0         4          /* animated core: cells 4..11         */
#define CORE_CELLS         8

#define PLAYER_MIN_X       0
#define PLAYER_MAX_X       (320 - 32)
#define PLAYER_MIN_Y       160
#define PLAYER_MAX_Y       (224 - 32)
#define PLAYER_SPEED       3

/* VRAM tile layout */
#define PLAYER_TILE        (TILE_USER_INDEX)              /* 16 tiles        */
#define BULLET_TILE        (PLAYER_TILE + 16)             /* 1 tile          */
#define BOSS_STATIC_TILE   (BULLET_TILE + 1)              /* 256 tiles       */
#define BOSS_CORE_TILE     (BOSS_STATIC_TILE + 256)       /* 8 * 64 tiles    */

/* ------------------------------------------------------------------------ */
/* Palettes                                                                 */
/* ------------------------------------------------------------------------ */
static const u32 bossColors[10] =
{
    0x000020,   /* 0 backdrop / transparent */
    0x200818,   /* 1 outline                */
    0x7040A0,   /* 2 plating light          */
    0x5A3080,   /* 3 plating dark           */
    0xF0F0F0,   /* 4 white                  */
    0xE02020,   /* 5 red                    */
    0x40E0F0,   /* 6 core ring bright       */
    0x2060C0,   /* 7 core ring dark         */
    0xF0E040,   /* 8 yellow                 */
    0xA0A0B0    /* 9 grey                   */
};

static const u32 shipColors[9] =
{
    0x000000,   /* 0 transparent */
    0x103040,   /* 1 outline     */
    0x60A0F0,   /* 2 hull        */
    0xF0F0F0,   /* 3 white       */
    0xF08020,   /* 4 orange      */
    0xF0E040,   /* 5 yellow      */
    0xF03030,   /* 6 bullet red  */
    0xF0A0A0,   /* 7 bullet core */
    0x40F0F0    /* 8 cockpit     */
};

/* ------------------------------------------------------------------------ */
/* Procedural graphics                                                      */
/* ------------------------------------------------------------------------ */
static const s16 orbitX[8] = {  36,  25,   0, -25, -36, -25,   0,  25 };
static const s16 orbitY[8] = {   0,  25,  36,  25,   0, -25, -36, -25 };

/* Colour of boss pixel (x,y) in 0..127 for core animation frame f */
static u8 bossPixel(s16 x, s16 y, u8 f)
{
    s16 dx = (x << 1) - 127;            /* doubled coords, centre = 0 */
    s16 dy = (y << 1) - 127;
    s16 ax = (dx < 0) ? -dx : dx;
    s16 ay = (dy < 0) ? -dy : dy;
    s16 d;
    s32 e, ex, ey;

    /* octagonal hull */
    if (ax > 124 || ay > 124 || (ax + ay) > 200) return 0;

    /* animated core */
    d = (ax > ay) ? (ax + (ay >> 1)) : (ay + (ax >> 1));
    if (d <= 60)
    {
        if (d >= 54) return 1;
        ex = dx - orbitX[f];
        ey = dy - orbitY[f];
        e = ex * ex + ey * ey;
        if (e <= 36) return 4;
        if (e <= 100) return 8;
        if (d <= 10) return 5;
        return ((((d >> 1) - f) & 7) < 4) ? 6 : 7;     /* expanding rings */
    }

    /* eyes */
    ex = ax - 52;
    ey = dy + 84;
    e = ex * ex + ey * ey;
    if (e <= 324)
    {
        if (e <= 64)  return 5;
        if (e <= 256) return 4;
        return 1;
    }

    /* cannons */
    if (ay >= 100 && ax >= 40 && ax <= 76)
        return (ay >= 112 && ax >= 50 && ax <= 66) ? 1 : 9;

    /* outline */
    if (ax >= 118 || ay >= 118 || (ax + ay) >= 192) return 1;

    /* rivets + plating */
    if (((x & 7) == 3) && ((y & 7) == 3)) return 9;
    return (((x >> 3) ^ (y >> 3)) & 1) ? 2 : 3;
}

/* Player ship pixel (x,y) in 0..31 */
static u8 shipPixel(s16 x, s16 y)
{
    s16 xx = (x << 1) - 31;
    s16 ax = (xx < 0) ? -xx : xx;
    s16 hw = 1 + (y >> 1);
    s16 lim;

    if (hw > 15) hw = 15;
    lim = (hw << 1) + 1;
    if (ax > lim) return 0;

    if (ax == lim || y == 0) return 1;                       /* outline   */
    if (y >= 9 && y <= 17 && ax <= 5)                        /* cockpit   */
        return (y <= 10 && ax <= 1) ? 3 : 8;
    if (y == 31 && ax <= 1) return 5;                        /* flame tip */
    if (y >= 28 && ax <= 5) return 4;                        /* flame     */
    if (ax >= 21 && y >= 22) return 4;                       /* wing tips */
    if (ax <= 1) return 3;                                   /* stripe    */
    return 2;
}

static u8 bulletPixel(s16 x, s16 y)
{
    s16 ex = (x << 1) - 7;
    s16 ey = (y << 1) - 7;
    s16 e = ex * ex + ey * ey;
    if (e <= 10) return 7;
    if (e <= 40) return 6;
    return 0;
}

/* Build one 8x8 boss tile (row-major image order) */
static void buildBossTile(u32 *out, u16 tx, u16 ty, u8 f)
{
    u16 r, c;
    for (r = 0; r < 8; r++)
    {
        u32 row = 0;
        s16 y = (ty << 3) + r;
        for (c = 0; c < 8; c++)
            row = (row << 4) | bossPixel((tx << 3) + c, y, f);
        out[r] = row;
    }
}

static void buildGraphics(void)
{
    u32 tile[8];
    u16 tx, ty, r, c, f;

    /* player: 4x4 tiles, stored column-major as the VDP sprite expects */
    for (tx = 0; tx < 4; tx++)
    {
        for (ty = 0; ty < 4; ty++)
        {
            for (r = 0; r < 8; r++)
            {
                u32 row = 0;
                for (c = 0; c < 8; c++)
                    row = (row << 4) | shipPixel((tx << 3) + c, (ty << 3) + r);
                tile[r] = row;
            }
            VDP_loadTileData(tile, PLAYER_TILE + tx * 4 + ty, 1, CPU);
        }
    }

    /* bullet */
    for (r = 0; r < 8; r++)
    {
        u32 row = 0;
        for (c = 0; c < 8; c++)
            row = (row << 4) | bulletPixel(c, r);
        tile[r] = row;
    }
    VDP_loadTileData(tile, BULLET_TILE, 1, CPU);

    /* boss static tiles (everything except the animated core block) */
    for (ty = 0; ty < BOSS_CELLS; ty++)
    {
        for (tx = 0; tx < BOSS_CELLS; tx++)
        {
            if (tx >= CORE_CELL0 && tx < CORE_CELL0 + CORE_CELLS &&
                ty >= CORE_CELL0 && ty < CORE_CELL0 + CORE_CELLS) continue;
            buildBossTile(tile, tx, ty, 0);
            VDP_loadTileData(tile, BOSS_STATIC_TILE + ty * BOSS_CELLS + tx, 1, CPU);
        }
    }

    /* boss core tiles: 8 animation frames x 8x8 cells */
    for (f = 0; f < BOSS_ANIM_FRAMES; f++)
    {
        for (ty = 0; ty < CORE_CELLS; ty++)
        {
            for (tx = 0; tx < CORE_CELLS; tx++)
            {
                buildBossTile(tile, CORE_CELL0 + tx, CORE_CELL0 + ty, f);
                VDP_loadTileData(tile, BOSS_CORE_TILE + f * 64 + ty * CORE_CELLS + tx, 1, CPU);
            }
        }
    }
}

/* ------------------------------------------------------------------------ */
/* Boss map                                                                 */
/* ------------------------------------------------------------------------ */
static void drawBossFull(u8 frame)
{
    u16 cx, cy;
    for (cy = 0; cy < BOSS_CELLS; cy++)
    {
        for (cx = 0; cx < BOSS_CELLS; cx++)
        {
            u16 tile;
            if (cx >= CORE_CELL0 && cx < CORE_CELL0 + CORE_CELLS &&
                cy >= CORE_CELL0 && cy < CORE_CELL0 + CORE_CELLS)
                tile = BOSS_CORE_TILE + frame * 64 + (cy - CORE_CELL0) * CORE_CELLS + (cx - CORE_CELL0);
            else
                tile = BOSS_STATIC_TILE + cy * BOSS_CELLS + cx;
            VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, tile),
                             BOSS_CELL_X + cx, BOSS_CELL_Y + cy);
        }
    }
}

/* Only the 64 animated cells change; called right after vblank starts */
static void drawBossCore(u8 frame)
{
    u16 cx, cy;
    for (cy = 0; cy < CORE_CELLS; cy++)
    {
        for (cx = 0; cx < CORE_CELLS; cx++)
        {
            u16 tile = BOSS_CORE_TILE + frame * 64 + cy * CORE_CELLS + cx;
            VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, tile),
                             BOSS_CELL_X + CORE_CELL0 + cx, BOSS_CELL_Y + CORE_CELL0 + cy);
        }
    }
}

/* ------------------------------------------------------------------------ */
/* Game state                                                               */
/* ------------------------------------------------------------------------ */
static s16 bx[NUM_BULLETS];
static s16 by[NUM_BULLETS];
static s16 bvx[NUM_BULLETS];
static s16 bvy[NUM_BULLETS];
static u16 rng = 0xACE1;

static u16 nextRand(void)
{
    rng ^= rng << 7;
    rng ^= rng >> 9;
    rng ^= rng << 8;
    return rng;
}

static void respawnBullet(u16 i)
{
    bx[i] = 100 + (nextRand() & 127);        /* inside the boss body */
    by[i] = 124;                             /* near the cannons     */
}

int main(bool hardReset)
{
    u16 i;
    s16 px = 144, py = PLAYER_MAX_Y;
    u8 tick = 0, frame = 0;
    u16 playerAttr, bulletAttr;
    u16 frameCount = 0;

    (void)hardReset;

    VDP_setScreenWidth320();
    VDP_setEnable(FALSE);

    for (i = 0; i < 10; i++) PAL_setColor(i, RGB24_TO_VDPCOLOR(bossColors[i]));
    PAL_setColor(16, 0x000);
    for (i = 1; i < 9; i++) PAL_setColor(16 + i, RGB24_TO_VDPCOLOR(shipColors[i]));

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    buildGraphics();
    drawBossFull(0);

    /* bullets: staggered heights/speeds so the sprite load per line stays low */
    for (i = 0; i < NUM_BULLETS; i++)
    {
        bx[i]  = 104 + i * 6;
        by[i]  = 120 + i * 6;
        bvy[i] = 1 + (i & 3);
        bvx[i] = (s16)(i % 3) - 1;
    }

    playerAttr = TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, PLAYER_TILE);
    bulletAttr = TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, BULLET_TILE);

    VDP_resetSprites();
    VDP_setEnable(TRUE);

    while (TRUE)
    {
        u16 joy = JOY_readJoypad(JOY_1);

        /* player */
        if (joy & BUTTON_LEFT)  px -= PLAYER_SPEED;
        if (joy & BUTTON_RIGHT) px += PLAYER_SPEED;
        if (joy & BUTTON_UP)    py -= PLAYER_SPEED;
        if (joy & BUTTON_DOWN)  py += PLAYER_SPEED;
        if (px < PLAYER_MIN_X) px = PLAYER_MIN_X;
        if (px > PLAYER_MAX_X) px = PLAYER_MAX_X;
        if (py < PLAYER_MIN_Y) py = PLAYER_MIN_Y;
        if (py > PLAYER_MAX_Y) py = PLAYER_MAX_Y;

        /* bullets */
        frameCount++;
        for (i = 0; i < NUM_BULLETS; i++)
        {
            by[i] += bvy[i];
            if (frameCount & 1) bx[i] += bvx[i];
            if (by[i] > 224 || bx[i] < 0 || bx[i] > 312) respawnBullet(i);
        }

        /* sprites: 0 = player, 1..16 = bullets (17 total, linked list) */
        VDP_setSpriteFull(0, px, py, SPRITE_SIZE(4, 4), playerAttr, 1);
        for (i = 0; i < NUM_BULLETS; i++)
        {
            VDP_setSpriteFull(i + 1, bx[i], by[i], SPRITE_SIZE(1, 1), bulletAttr,
                              (i == NUM_BULLETS - 1) ? 0 : (i + 2));
        }
        VDP_updateSprites(1 + NUM_BULLETS, DMA_QUEUE);

        /* boss animation: advance every 4 video frames */
        tick++;
        if (tick >= BOSS_ANIM_DELAY)
        {
            tick = 0;
            frame = (frame + 1) & (BOSS_ANIM_FRAMES - 1);
            SYS_doVBlankProcess();          /* wait vblank, flush DMA queue  */
            drawBossCore(frame);            /* 64 map writes, still in vblank */
        }
        else
        {
            SYS_doVBlankProcess();
        }
    }

    return 0;
}
