/*
 * main.c - SGDK 2.11 boss fight demo (no resource files)
 *
 *  - 128x128 boss, 8 animation frames, frame changes every 4 video frames.
 *    The boss is horizontally symmetric, so only its LEFT half (64x128 = 128 tiles)
 *    is stored per frame. The right half reuses the same tiles with the H-flip bit.
 *    8 frames x 128 tiles = 1024 tiles, all loaded once at startup, so animating
 *    is just changing the tile base in the 16 boss sprites (no DMA while playing).
 *  - 16 falling 8x8 bullets
 *  - 32x32 player ship, D-pad on pad 1, confined to the bottom of the screen
 *  - All graphics are generated in code at startup
 *
 * VRAM use: ~1060 tiles from TILE_USER_INDEX.
 */

#include <genesis.h>

/* 8-bit RGB -> Mega Drive 9-bit colour */
#define RGBC(r, g, b) ((u16)((((b) >> 5) << 9) | (((g) >> 5) << 5) | (((r) >> 5) << 1)))

/* ---------- VRAM layout ---------- */
#define T_STAR      (TILE_USER_INDEX + 0)
#define T_BULLET    (TILE_USER_INDEX + 1)
#define T_SHIP      (TILE_USER_INDEX + 2)      /* 2 frames x 16 tiles */
#define T_BOSS      (TILE_USER_INDEX + 34)     /* 8 frames x 128 tiles */
#define BOSS_FRAME_TILES 128

/* ---------- Sprite table layout ---------- */
#define SPR_PLAYER  0
#define SPR_BULLET0 1
#define NUM_BULLETS 16
#define SPR_BOSS0   (SPR_BULLET0 + NUM_BULLETS)   /* 17 */
#define NUM_BOSS_SPR 16
#define NUM_SPRITES (SPR_BOSS0 + NUM_BOSS_SPR)    /* 33 */

#define BOSS_MIN_X  8
#define BOSS_MAX_X  184
#define BOSS_BASE_Y 8

#define PLAYER_MIN_Y 144
#define PLAYER_MAX_Y 184

/* ---------- Palettes ---------- */
static const u16 pal_boss[16] = {
    0, RGBC(16, 8, 24), RGBC(64, 40, 96), RGBC(104, 72, 144),
    RGBC(152, 120, 192), RGBC(232, 232, 232), RGBC(200, 32, 32), RGBC(255, 224, 40),
    RGBC(96, 24, 24), RGBC(72, 96, 128), RGBC(112, 152, 192), RGBC(200, 184, 120),
    0, RGBC(255, 144, 32), 0, 0
};

static const u16 pal_ship[16] = {
    0, RGBC(32, 32, 64), RGBC(224, 224, 240), RGBC(144, 160, 200),
    RGBC(80, 96, 144), RGBC(64, 224, 255), RGBC(255, 160, 32), RGBC(255, 240, 120),
    RGBC(224, 40, 40), 0, 0, 0, 0, 0, 0, 0
};

static const u16 pal_bullet[16] = {
    0, RGBC(180, 20, 20), RGBC(255, 100, 30), RGBC(255, 240, 170),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* Star tile (palette 0, colours 14/15). One nibble per pixel. */
static const u32 star_tile[8] = {
    0x00000000, 0x00E00000, 0x0EFE0000, 0x00E00000,
    0x00000000, 0x00000000, 0x000000E0, 0x00000000
};

/* ---------- Small helpers ---------- */
static u16 rseed = 0x1234;

static u16 rnd(u16 n)
{
    rseed = rseed * 25173u + 13849u;
    return (rseed >> 5) % n;
}

static inline s16 iabs(s16 v) { return (v < 0) ? -v : v; }

/* ---------- Boss tile generation ---------- */
static u32 tbuf[BOSS_FRAME_TILES * 8];     /* one frame of the boss' left half (4 KB) */
static u8  mask[130 * 66];                 /* silhouette with 1px border + mirror column */
static s32 dx2sq[64];                      /* (127-2x)^2 */
static s16 wy1[44], wy2[44], wth[44];      /* wing spines + thickness per column */

static const s8 wflap[8] = { -16, -10, -2, 6, 14, 6, -2, -10 };
static const s8 poff[8]  = { -6, -3, 0, 3, 6, 3, 0, -3 };
static const s16 lcx[4]  = { 39, 18, 18, 39 };
static const s16 lcy[4]  = { 22, 47, 80, 105 };

/* x,y inside the 64x128 left half. Layout: 2x4 sprites of 4x4 tiles, column-major tiles. */
static void put_boss(s16 x, s16 y, u8 c)
{
    u16 si = (u16)((y >> 5) * 2 + (x >> 5));
    u16 t  = si * 16 + ((x & 31) >> 3) * 4 + ((y & 31) >> 3);
    u16 w  = t * 8 + (y & 7);
    tbuf[w] |= ((u32)c) << ((7 - (x & 7)) * 4);
}

static u8 boss_shade(u16 f, s16 x, s16 y)
{
    s16 dy2 = 2 * y - 127;
    s32 dsq = dx2sq[x] + (s32)dy2 * dy2;
    s16 i, lit, t;
    s32 di;

    /* Outside the round body: wings or horn */
    if (dsq >= 14884)
    {
        if (y < 20 && x >= 24 && x <= 36) return 11;
        return (((x + y) >> 3) & 1) ? 10 : 9;
    }

    /* Eye with moving iris/pupil */
    if (dsq < 2304)
    {
        if (dsq >= 1936) return 1;
        t = dy2 - (s16)(poff[f] * 2);
        di = dx2sq[x] + (s32)t * t;
        if (di < 150) return 1;
        if (di < 400) return 13;
        if (di < 784) return 6;
        return 5;
    }

    /* Chasing lights (ping-pong) */
    lit = (f < 4) ? f : (7 - f);
    for (i = 0; i < 4; i++)
    {
        s16 ddx = x - lcx[i];
        s16 ddy = y - lcy[i];
        s16 dd = ddx * ddx + ddy * ddy;
        if (dd < 49)
        {
            if (dd >= 36) return 1;
            if (i == lit) return (dd < 12) ? 7 : 13;
            return (dd < 12) ? 2 : 8;
        }
    }

    /* Armour plating */
    if (dsq >= 12996) return 2;
    if (dsq >= 11664) return 4;
    if (x == 63 || (y & 31) == 0) return 2;
    return 3;
}

static void gen_boss_frame(u16 f)
{
    s16 x, y, k;
    s16 flap = wflap[f];

    memset(mask, 0, sizeof(mask));
    memset(tbuf, 0, sizeof(tbuf));

    for (x = 0; x < 44; x++)
    {
        k = 44 - x;
        wy1[x] = 40 + (flap * k) / 44;
        wy2[x] = 88 - (flap * k) / 44;
        wth[x] = 3 + (x >> 2);
    }

    /* Pass 1: silhouette */
    for (y = 0; y < 128; y++)
    {
        s16 dy2 = 2 * y - 127;
        s32 dysq = (s32)dy2 * dy2;
        u8 *row = &mask[(y + 1) * 66];

        for (x = 0; x < 64; x++)
        {
            u8 in = 0;
            if (dx2sq[x] + dysq < 14884) in = 1;
            else if (x < 44 && (iabs(y - wy1[x]) <= wth[x] || iabs(y - wy2[x]) <= wth[x])) in = 1;
            if (!in && y < 20 && x >= 24 && x <= 36 && iabs(x - 30) * 2 <= y) in = 1;
            row[x + 1] = in;
        }
        row[65] = row[64];      /* mirror column so the centre seam has no outline */
    }

    /* Pass 2: colour (automatic 1px outline from the silhouette) */
    for (y = 0; y < 128; y++)
    {
        for (x = 0; x < 64; x++)
        {
            u16 idx = (y + 1) * 66 + x + 1;
            u8 c;
            if (!mask[idx]) continue;
            if (!mask[idx - 1] || !mask[idx + 1] || !mask[idx - 66] || !mask[idx + 66]) c = 1;
            else c = boss_shade(f, x, y);
            put_boss(x, y, c);
        }
    }
}

/* ---------- Player ship tile generation ---------- */
static u32 sbuf[16 * 8];

static void put_ship(s16 x, s16 y, u8 c)
{
    u16 t = (x >> 3) * 4 + (y >> 3);        /* column-major 4x4 tiles */
    sbuf[t * 8 + (y & 7)] |= ((u32)c) << ((7 - (x & 7)) * 4);
}

static u8 ship_hull(s16 x, s16 y)
{
    s16 dx2, lim;
    if (x < 0 || x > 31 || y < 0 || y > 29) return 0;
    dx2 = iabs(2 * x - 31);
    if (y <= 27)
    {
        lim = 3 + y;
        if (lim > 13) lim = 13;
        if (dx2 <= lim) return 1;
    }
    if (y >= 12 && y <= 28 && dx2 >= 9 && y * 22 >= 264 + (dx2 - 9) * 10) return 1;
    if (y >= 28 && dx2 >= 3 && dx2 <= 9) return 1;     /* nozzle */
    return 0;
}

static void gen_ship_frame(u16 fr)
{
    s16 x, y, dx2;
    memset(sbuf, 0, sizeof(sbuf));

    for (y = 0; y < 32; y++)
    {
        for (x = 0; x < 32; x++)
        {
            u8 c = 0;
            dx2 = iabs(2 * x - 31);

            if (y >= 30)
            {
                /* engine flame, flickers between two frames */
                if (y == 30 && dx2 >= 3 && dx2 <= (fr ? 7 : 5)) c = (dx2 <= 3) ? 7 : 6;
                else if (y == 31 && fr && dx2 >= 3 && dx2 <= 3) c = 6;
            }
            else if (ship_hull(x, y))
            {
                if (!ship_hull(x - 1, y) || !ship_hull(x + 1, y) ||
                    !ship_hull(x, y - 1) || !ship_hull(x, y + 1))
                    c = 1;
                else if (dx2 <= 5 && y >= 7 && y <= 13) c = 5;                   /* cockpit */
                else if (dx2 <= 7 && y >= 6 && y <= 14) c = 4;                   /* cockpit rim */
                else if (dx2 <= 3) c = 2;                                        /* centre stripe */
                else if (y >= 12 && dx2 >= 17 && dx2 <= 21) c = 8;               /* red wing stripe */
                else if (y >= 12 && dx2 >= 27) c = 4;                            /* wing tips */
                else if (y >= 22 && dx2 <= 13) c = 4;                            /* rear shade */
                else c = 3;
            }
            if (c) put_ship(x, y, c);
        }
    }
    VDP_loadTileData(sbuf, T_SHIP + fr * 16, 16, DMA);
}

/* ---------- Bullet tile ---------- */
static void gen_bullet_tile(void)
{
    u32 b[8];
    s16 x, y;
    for (y = 0; y < 8; y++)
    {
        b[y] = 0;
        for (x = 0; x < 8; x++)
        {
            s16 ax = 2 * x - 7, ay = 2 * y - 7;
            s16 d2 = ax * ax + ay * ay;
            u32 c = 0;
            if (d2 <= 10) c = 3;
            else if (d2 <= 26) c = 2;
            else if (d2 <= 40) c = 1;
            b[y] |= c << ((7 - x) * 4);
        }
    }
    VDP_loadTileData(b, T_BULLET, 1, CPU);
}

/* ---------- Game state ---------- */
static s16 bx[NUM_BULLETS], by[NUM_BULLETS];      /* 12.4 fixed point */
static s16 bvx[NUM_BULLETS], bvy[NUM_BULLETS];
static u16 bdelay[NUM_BULLETS];                   /* >0: waiting (hidden) */

static s16 bossX = 96, bossY = BOSS_BASE_Y, bossDir = 1;

static const s8 bobtab[8] = { 0, 1, 2, 3, 3, 2, 1, 0 };

static void spawn_bullet(u16 i)
{
    bx[i]  = (bossX + 20 + rnd(80)) << 4;
    by[i]  = (bossY + 96) << 4;
    bvy[i] = 24 + rnd(32);
    bvx[i] = (s16)rnd(17) - 8;
}

static void kill_bullet(u16 i)
{
    bdelay[i] = 10 + rnd(50);
}

static void draw_hits(u16 hits)
{
    char str[8];
    intToStr(hits, str, 3);
    VDP_drawText("HITS", 1, 27);
    VDP_drawText(str, 6, 27);
}

int main(bool hardReset)
{
    u16 i, t = 0;
    u16 frame = 0, animTick = 0;
    s16 px = 144, py = 184;
    u16 hits = 0, inv = 0;
    u16 prevHits = 0xFFFF;

    (void)hardReset;

    VDP_setScreenWidth320();

    /* Palettes */
    PAL_setColor(0, 0x000);
    PAL_setColor(14, RGBC(100, 120, 180));
    PAL_setColor(15, RGBC(255, 255, 255));
    PAL_setPalette(PAL1, pal_boss, CPU);
    PAL_setPalette(PAL2, pal_ship, CPU);
    PAL_setPalette(PAL3, pal_bullet, CPU);

    VDP_drawText("GENERATING GRAPHICS...", 9, 12);

    /* Generate all graphics */
    for (i = 0; i < 64; i++)
    {
        s16 d = 127 - 2 * (s16)i;
        dx2sq[i] = (s32)d * d;
    }
    for (i = 0; i < 8; i++)
    {
        gen_boss_frame(i);
        VDP_loadTileData(tbuf, T_BOSS + i * BOSS_FRAME_TILES, BOSS_FRAME_TILES, DMA);
    }
    gen_ship_frame(0);
    gen_ship_frame(1);
    gen_bullet_tile();
    VDP_loadTileData(star_tile, T_STAR, 1, CPU);

    VDP_clearText(9, 12, 22);

    /* Starfield on plane B */
    for (i = 0; i < 48; i++)
        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, T_STAR), rnd(40), rnd(32));

    /* Bullets start staggered */
    for (i = 0; i < NUM_BULLETS; i++)
        bdelay[i] = 1 + i * 5 + rnd(20);

    VDP_resetSprites();

    while (TRUE)
    {
        u16 joy = JOY_readJoypad(JOY_1);

        /* ----- Player ----- */
        if (joy & BUTTON_LEFT)  px -= 3;
        if (joy & BUTTON_RIGHT) px += 3;
        if (joy & BUTTON_UP)    py -= 3;
        if (joy & BUTTON_DOWN)  py += 3;
        if (px < 0) px = 0;
        if (px > 288) px = 288;
        if (py < PLAYER_MIN_Y) py = PLAYER_MIN_Y;
        if (py > PLAYER_MAX_Y) py = PLAYER_MAX_Y;
        if (inv) inv--;

        /* ----- Boss ----- */
        if ((t & 1) == 0)
        {
            bossX += bossDir;
            if (bossX >= BOSS_MAX_X) { bossX = BOSS_MAX_X; bossDir = -1; }
            if (bossX <= BOSS_MIN_X) { bossX = BOSS_MIN_X; bossDir = 1; }
        }
        bossY = BOSS_BASE_Y + bobtab[(t >> 3) & 7];

        if (++animTick >= 4)
        {
            animTick = 0;
            frame = (frame + 1) & 7;
        }

        /* ----- Bullets ----- */
        for (i = 0; i < NUM_BULLETS; i++)
        {
            if (bdelay[i])
            {
                bdelay[i]--;
                if (bdelay[i] == 0) spawn_bullet(i);
            }
            else
            {
                s16 bxp, byp;
                bx[i] += bvx[i];
                by[i] += bvy[i];
                bxp = bx[i] >> 4;
                byp = by[i] >> 4;

                if (byp > 224 || bxp < -8 || bxp > 320)
                {
                    kill_bullet(i);
                }
                else if (!inv &&
                         bxp + 8 > px + 6 && bxp < px + 26 &&
                         byp + 8 > py + 4 && byp < py + 28)
                {
                    hits++;
                    inv = 60;
                    kill_bullet(i);
                }
            }
        }

        /* ----- HUD ----- */
        if (hits != prevHits)
        {
            draw_hits(hits);
            prevHits = hits;
        }

        /* ----- Sprites ----- */
        {
            s16 sy = (inv && (inv & 4)) ? -40 : py;     /* blink while invulnerable */
            VDP_setSpriteFull(SPR_PLAYER, px, sy, SPRITE_SIZE(4, 4),
                              TILE_ATTR_FULL(PAL2, TRUE, FALSE, FALSE, T_SHIP + ((t >> 2) & 1) * 16),
                              SPR_BULLET0);
        }

        for (i = 0; i < NUM_BULLETS; i++)
        {
            s16 sx = bx[i] >> 4;
            s16 sy = by[i] >> 4;
            if (bdelay[i]) { sx = 0; sy = -40; }
            VDP_setSpriteFull(SPR_BULLET0 + i, sx, sy, SPRITE_SIZE(1, 1),
                              TILE_ATTR_FULL(PAL3, TRUE, FALSE, FALSE, T_BULLET),
                              SPR_BULLET0 + i + 1);
        }

        {
            u16 base = T_BOSS + frame * BOSS_FRAME_TILES;
            u16 n = SPR_BOSS0;
            u16 sxi, syi;
            for (syi = 0; syi < 4; syi++)
            {
                for (sxi = 0; sxi < 2; sxi++)
                {
                    u16 tile = base + (syi * 2 + sxi) * 16;
                    s16 y = bossY + syi * 32;

                    /* left half */
                    VDP_setSpriteFull(n, bossX + sxi * 32, y, SPRITE_SIZE(4, 4),
                                      TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, tile), n + 1);
                    n++;
                    /* right half: same tiles, mirrored */
                    VDP_setSpriteFull(n, bossX + 96 - sxi * 32, y, SPRITE_SIZE(4, 4),
                                      TILE_ATTR_FULL(PAL1, FALSE, FALSE, TRUE, tile),
                                      (n == NUM_SPRITES - 1) ? 0 : n + 1);
                    n++;
                }
            }
        }

        VDP_updateSprites(NUM_SPRITES, DMA_QUEUE);

        /* Scrolling starfield */
        VDP_setVerticalScroll(BG_B, -(s16)(t >> 1));

        t++;
        SYS_doVBlankProcess();
    }

    return 0;
}
