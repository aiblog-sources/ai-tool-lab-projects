/*
 * main.c - Bullet ring demo for Sega Mega Drive / Genesis (SGDK 2.11)
 *
 * - Enemy at screen center fires a ring of 32 bullets (8x8) every 30 frames.
 * - Max 64 bullets alive at once (a ring is skipped if it would not fit).
 * - Every other volley is rotated by half a step so the pattern interlocks.
 * - Player (16x16) moves with the d-pad on pad 1.
 * - No resources: all tiles are const u32 arrays loaded with VDP_loadTileData.
 * - Uses the low-level VDP sprite API (no sprite engine, no rescomp).
 */

#include <genesis.h>

#define MAX_BULLETS     64
#define RING_COUNT      32
#define FIRE_INTERVAL   30
#define BULLET_SPEED    3          /* pixels per frame */
#define PLAYER_SPEED    2

#define CENTER_X        160
#define CENTER_Y        112

#define PLAYER_W        16
#define PLAYER_H        16
#define ENEMY_W         16
#define ENEMY_H         16
#define BULLET_W        8
#define BULLET_H        8

/* Tile layout in VRAM */
#define TILE_BULLET     (TILE_USER_INDEX + 0)   /* 1 tile  */
#define TILE_PLAYER     (TILE_USER_INDEX + 1)   /* 4 tiles */
#define TILE_ENEMY      (TILE_USER_INDEX + 5)   /* 4 tiles */
#define TILE_TOTAL      9

/* Palette 1 colour indexes used by the tiles:
 * 1 white, 2 red, 3 yellow, 4 cyan, 5 magenta */

static const u32 gfxTiles[TILE_TOTAL * 8] =
{
    /* ---- Bullet 8x8 ---- */
    0x00222200,
    0x02233220,
    0x22333322,
    0x23333332,
    0x23333332,
    0x22333322,
    0x02233220,
    0x00222200,

    /* ---- Player 16x16 (sprite tile order is column-major: TL, BL, TR, BR) ---- */
    /* TL */
    0x00000004,
    0x00000044,
    0x00000044,
    0x00000441,
    0x00000441,
    0x00004444,
    0x00044444,
    0x00444444,
    /* BL */
    0x04444444,
    0x04444444,
    0x04444400,
    0x04444000,
    0x04400000,
    0x00000003,
    0x00000003,
    0x00000000,
    /* TR */
    0x40000000,
    0x44000000,
    0x44000000,
    0x14400000,
    0x14400000,
    0x44440000,
    0x44444000,
    0x44444400,
    /* BR */
    0x44444440,
    0x44444440,
    0x00444440,
    0x00044440,
    0x00000440,
    0x30000000,
    0x30000000,
    0x00000000,

    /* ---- Enemy 16x16 (TL, BL, TR, BR) ---- */
    /* TL */
    0x00000055,
    0x00005555,
    0x00055555,
    0x00555555,
    0x05555511,
    0x05555111,
    0x55555112,
    0x55555512,
    /* BL (upper half mirrored vertically) */
    0x55555512,
    0x55555112,
    0x05555111,
    0x05555511,
    0x00555555,
    0x00055555,
    0x00005555,
    0x00000055,
    /* TR */
    0x55000000,
    0x55550000,
    0x55555000,
    0x55555500,
    0x11555550,
    0x11155550,
    0x21155555,
    0x21555555,
    /* BR */
    0x21555555,
    0x21155555,
    0x11155550,
    0x11555550,
    0x55555500,
    0x55555000,
    0x55550000,
    0x55000000
};

/* cos(k * 11.25 deg) * 256 for k = 0..8 (first quadrant) */
static const s16 cosQuarter[9] = { 256, 251, 237, 213, 181, 142, 98, 50, 0 };

typedef struct
{
    s32 x;      /* 24.8 fixed point, sprite top-left */
    s32 y;
    s16 vx;     /* 8.8 fixed point velocity */
    s16 vy;
    u8  active;
} Bullet;

static Bullet bullets[MAX_BULLETS];
static u16 bulletCount;

static s16 dirX[RING_COUNT];   /* velocity vectors of the 32 ring directions */
static s16 dirY[RING_COUNT];

static s16 playerX, playerY;
static u16 fireTimer;
static u8  volleyFlip;

/* cos of k * 11.25 degrees, scaled by 256 */
static s16 cos32(u8 k)
{
    k &= 31;
    if (k <= 8)  return  cosQuarter[k];
    if (k <= 16) return -cosQuarter[16 - k];
    if (k <= 24) return -cosQuarter[k - 16];
    return cosQuarter[32 - k];
}

static void buildDirections(void)
{
    u8 k;
    for (k = 0; k < RING_COUNT; k++)
    {
        dirX[k] = cos32(k) * BULLET_SPEED;
        dirY[k] = cos32(k + 24) * BULLET_SPEED;   /* sin(a) = cos(a - 90deg) */
    }
}

static void fireRing(void)
{
    u8 k;
    u16 slot = 0;

    /* Only fire a whole ring, never a partial one */
    if (bulletCount + RING_COUNT > MAX_BULLETS) return;

    for (k = 0; k < RING_COUNT; k++)
    {
        s32 vx = dirX[k];
        s32 vy = dirY[k];

        if (volleyFlip)
        {
            /* rotate by 5.625 degrees: cos = 255/256, sin = 25/256 */
            s32 rx = (vx * 255 - vy * 25) >> 8;
            s32 ry = (vx * 25 + vy * 255) >> 8;
            vx = rx;
            vy = ry;
        }

        while (slot < MAX_BULLETS && bullets[slot].active) slot++;
        if (slot >= MAX_BULLETS) break;

        bullets[slot].x = ((s32) (CENTER_X - BULLET_W / 2)) << 8;
        bullets[slot].y = ((s32) (CENTER_Y - BULLET_H / 2)) << 8;
        bullets[slot].vx = (s16) vx;
        bullets[slot].vy = (s16) vy;
        bullets[slot].active = TRUE;
        bulletCount++;
    }

    volleyFlip ^= 1;
}

static void updateBullets(void)
{
    u16 i;
    for (i = 0; i < MAX_BULLETS; i++)
    {
        Bullet *b = &bullets[i];
        s16 sx, sy;

        if (!b->active) continue;

        b->x += b->vx;
        b->y += b->vy;

        sx = (s16) (b->x >> 8);
        sy = (s16) (b->y >> 8);

        if (sx < -BULLET_W || sx > 320 || sy < -BULLET_H || sy > 224)
        {
            b->active = FALSE;
            bulletCount--;
        }
    }
}

static void updatePlayer(void)
{
    u16 joy = JOY_readJoypad(JOY_1);

    if (joy & BUTTON_LEFT)  playerX -= PLAYER_SPEED;
    if (joy & BUTTON_RIGHT) playerX += PLAYER_SPEED;
    if (joy & BUTTON_UP)    playerY -= PLAYER_SPEED;
    if (joy & BUTTON_DOWN)  playerY += PLAYER_SPEED;

    if (playerX < 0) playerX = 0;
    if (playerX > 320 - PLAYER_W) playerX = 320 - PLAYER_W;
    if (playerY < 0) playerY = 0;
    if (playerY > 224 - PLAYER_H) playerY = 224 - PLAYER_H;
}

static void drawSprites(void)
{
    const u16 bulletAttr = TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, TILE_BULLET);
    const u16 playerAttr = TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, TILE_PLAYER);
    const u16 enemyAttr  = TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, TILE_ENEMY);
    u16 n = 0;
    u16 i;

    /* Lower sprite index = drawn on top: player, then bullets, then enemy */
    VDP_setSpriteFull(n, playerX, playerY, SPRITE_SIZE(2, 2), playerAttr, n + 1);
    n++;

    for (i = 0; i < MAX_BULLETS; i++)
    {
        if (!bullets[i].active) continue;
        VDP_setSpriteFull(n, (s16) (bullets[i].x >> 8), (s16) (bullets[i].y >> 8),
                          SPRITE_SIZE(1, 1), bulletAttr, n + 1);
        n++;
    }

    /* Enemy is last in the list: link = 0 terminates it */
    VDP_setSpriteFull(n, CENTER_X - ENEMY_W / 2, CENTER_Y - ENEMY_H / 2,
                      SPRITE_SIZE(2, 2), enemyAttr, 0);
    n++;

    VDP_updateSprites(n, DMA_QUEUE);
}

static void drawHud(void)
{
    static u16 lastShown = 0xFFFF;
    char str[4];

    if (lastShown == bulletCount) return;
    lastShown = bulletCount;

    intToStr(bulletCount, str, 2);
    VDP_drawText("BULLETS:", 1, 1);
    VDP_drawText(str, 10, 1);
}

int main(bool hardReset)
{
    u16 i;
    (void) hardReset;

    /* 320x224 (H40, NTSC) */
    VDP_setScreenWidth320();

    /* Palette: PAL0 for text/background, PAL1 for sprites */
    PAL_setColor(0,  0x0200);   /* dark blue background */
    PAL_setColor(15, 0x0EEE);   /* white font colour    */
    PAL_setColor(17, 0x0EEE);   /* 1 white   */
    PAL_setColor(18, 0x002E);   /* 2 red     */
    PAL_setColor(19, 0x00EE);   /* 3 yellow  */
    PAL_setColor(20, 0x0EE0);   /* 4 cyan    */
    PAL_setColor(21, 0x0E0E);   /* 5 magenta */

    /* Load all tiles from code */
    VDP_loadTileData(gfxTiles, TILE_USER_INDEX, TILE_TOTAL, CPU);

    VDP_resetSprites();

    buildDirections();

    for (i = 0; i < MAX_BULLETS; i++) bullets[i].active = FALSE;
    bulletCount = 0;

    playerX = CENTER_X - PLAYER_W / 2;
    playerY = 190;
    fireTimer = 0;
    volleyFlip = 0;

    while (TRUE)
    {
        updatePlayer();

        if (++fireTimer >= FIRE_INTERVAL)
        {
            fireTimer = 0;
            fireRing();
        }

        updateBullets();
        drawSprites();
        drawHud();

        SYS_doVBlankProcess();
    }

    return 0;
}
