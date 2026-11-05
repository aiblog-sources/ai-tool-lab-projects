/*
 * main.c - Radial bullet ring demo for Sega Mega Drive / Genesis (SGDK 2.11)
 *
 * - Enemy at screen center fires a ring of 32 bullets (8x8) every 30 frames.
 * - Max 64 bullets alive at once. A ring is spawned only if all 32 slots are free,
 *   so rings never come out with holes.
 * - Player moves with the d-pad (pad 1).
 * - No resource files: all tiles and palettes are const arrays in this file.
 *
 * Hardware sprite budget (H40: 80 sprites total, 20 sprites / 320 px per scanline):
 *   - Sprite list is a fixed chain of 66 entries (player, enemy, 64 bullets).
 *     It is rewritten and DMA'd every frame, and unused bullets are parked off-screen.
 *     The draw order never changes, so there is no flicker or shuffling.
 *   - Bullet speed is 3 px/frame, so a ring leaves the screen in <= ~51 frames.
 *     That means at most 2 rings (64 bullets) are alive at once, and the pool never starves.
 *   - Worst-case scanline: a freshly spawned ring (r=16) puts at most ~11 bullets
 *     on one line near its top/bottom. The previous ring (r~106) adds ~4, plus
 *     the player and enemy, for ~17 sprites. This stays under the 20/line limit,
 *     so nothing drops out on real hardware.
 *   - The player is sprite 0, so it has the highest priority in any case.
 */

#include <genesis.h>

/* ------------------------------------------------------------------------- */
/* Configuration                                                             */
/* ------------------------------------------------------------------------- */
#define SCREEN_W        320
#define SCREEN_H        224
#define CENTER_X        160
#define CENTER_Y        112

#define MAX_BULLETS     64
#define RING_BULLETS    32
#define FIRE_INTERVAL   30          /* frames between rings                   */
#define BULLET_SPEED    3           /* pixels per frame                       */
#define START_RADIUS    16          /* ring spawn radius in pixels            */
#define PLAYER_SPEED    2

/* Sprite list layout (index = draw priority, lower = on top) */
#define SPR_PLAYER      0
#define SPR_ENEMY       1
#define SPR_BULLET0     2
#define SPRITE_COUNT    (SPR_BULLET0 + MAX_BULLETS)     /* 66 <= 80 */

#define HIDDEN_X        0
#define HIDDEN_Y        (-64)       /* completely above the visible area      */

/* VRAM tile layout */
#define TILE_BULLET     (TILE_USER_INDEX + 0)           /* 1 tile  (8x8)      */
#define TILE_PLAYER     (TILE_USER_INDEX + 1)           /* 1 tile  (8x8)      */
#define TILE_ENEMY      (TILE_USER_INDEX + 2)           /* 4 tiles (16x16)    */

/* ------------------------------------------------------------------------- */
/* Graphics data (4bpp, one u32 per 8-pixel row, high nibble = leftmost)     */
/* ------------------------------------------------------------------------- */

/* Bullet: round ball, colors 1=outline 2=mid 3=core */
static const u32 bulletTile[8] =
{
    0x00111100,
    0x01222210,
    0x12233221,
    0x12333321,
    0x12333321,
    0x12233221,
    0x01222210,
    0x00111100
};

/* Player: small ship, colors 1=dark 2=mid 3=light */
static const u32 playerTile[8] =
{
    0x00033000,
    0x00033000,
    0x00333300,
    0x01333310,
    0x01233210,
    0x12233221,
    0x12200221,
    0x11000011
};

/* Enemy: 16x16 orb. Hardware sprites store tiles column-major:
 * top-left, bottom-left, top-right, bottom-right. */
static const u32 enemyTiles[32] =
{
    /* top-left */
    0x00000111, 0x00011122, 0x00112222, 0x01122223,
    0x01122233, 0x11222333, 0x12223333, 0x12223334,
    /* bottom-left */
    0x12223334, 0x12223333, 0x11222333, 0x01122233,
    0x01122223, 0x00112222, 0x00011122, 0x00000111,
    /* top-right */
    0x11100000, 0x22111000, 0x22221100, 0x32222110,
    0x33222110, 0x33322211, 0x33332221, 0x43332221,
    /* bottom-right */
    0x43332221, 0x33332221, 0x33322211, 0x33222110,
    0x32222110, 0x22221100, 0x22111000, 0x11100000
};

/* Palettes (0x0BGR) - color 0 is transparent */
static const u16 bulletPal[4] = { 0x0000, 0x0008, 0x004E, 0x06EE };
static const u16 playerPal[4] = { 0x0000, 0x0822, 0x0EC4, 0x0EEE };
static const u16 enemyPal[5]  = { 0x0000, 0x0606, 0x0C2C, 0x0E8E, 0x00EE };

/* ------------------------------------------------------------------------- */
/* Trig: first quadrant of cos(i * 5.625 deg) * 256, i = 0..16               */
/* ------------------------------------------------------------------------- */
static const s16 cosQuarter[17] =
{
    256, 255, 251, 245, 237, 226, 213, 198, 181,
    162, 142, 121,  98,  74,  50,  25,   0
};

/* 64 steps per full circle, result in 1/256 units */
static s16 cos64(u16 i)
{
    i &= 63;
    if (i <= 16) return  cosQuarter[i];
    if (i <= 32) return -cosQuarter[32 - i];
    if (i <= 48) return -cosQuarter[i - 32];
    return cosQuarter[64 - i];
}

static s16 sin64(u16 i)
{
    return cos64(i + 48);       /* sin(a) = cos(a - 90 deg) */
}

/* ------------------------------------------------------------------------- */
/* Game state                                                                */
/* ------------------------------------------------------------------------- */
typedef struct
{
    s32 x, y;           /* center position, 8.8 fixed point */
    s16 vx, vy;         /* velocity, 8.8 fixed point / frame */
    u8  active;
} Bullet;

static Bullet bullets[MAX_BULLETS];
static u16 activeCount;
static u16 fireTimer;
static u16 ringParity;
static s16 playerX, playerY;    /* top-left, pixels */

/* ------------------------------------------------------------------------- */
/* Logic                                                                     */
/* ------------------------------------------------------------------------- */
static void spawnRing(void)
{
    u16 i, k, slot, phase;
    u16 freeSlots = 0;

    for (i = 0; i < MAX_BULLETS; i++)
        if (!bullets[i].active) freeSlots++;

    /* All or nothing: never emit a partial ring */
    if (freeSlots < RING_BULLETS) return;

    phase = ringParity;         /* every other ring is offset by half a step */
    ringParity ^= 1;

    slot = 0;
    for (k = 0; k < RING_BULLETS; k++)
    {
        u16 a = (u16)(k * 2 + phase);
        s16 c = cos64(a);
        s16 s = sin64(a);
        Bullet *b;

        while (bullets[slot].active) slot++;
        b = &bullets[slot++];

        b->x  = ((s32)CENTER_X << 8) + (s32)c * START_RADIUS;
        b->y  = ((s32)CENTER_Y << 8) + (s32)s * START_RADIUS;
        b->vx = (s16)(c * BULLET_SPEED);
        b->vy = (s16)(s * BULLET_SPEED);
        b->active = 1;
    }
}

static void updateBullets(void)
{
    u16 i, count = 0;

    for (i = 0; i < MAX_BULLETS; i++)
    {
        Bullet *b = &bullets[i];
        s16 px, py;

        if (!b->active) continue;

        b->x += b->vx;
        b->y += b->vy;

        px = (s16)(b->x >> 8);
        py = (s16)(b->y >> 8);

        /* Free the slot as soon as the 8x8 sprite is fully off-screen */
        if (px <= -4 || px >= SCREEN_W + 4 || py <= -4 || py >= SCREEN_H + 4)
        {
            b->active = 0;
            continue;
        }
        count++;
    }
    activeCount = count;
}

static void updatePlayer(void)
{
    u16 j = JOY_readJoypad(JOY_1);

    if (j & BUTTON_LEFT)  playerX -= PLAYER_SPEED;
    if (j & BUTTON_RIGHT) playerX += PLAYER_SPEED;
    if (j & BUTTON_UP)    playerY -= PLAYER_SPEED;
    if (j & BUTTON_DOWN)  playerY += PLAYER_SPEED;

    if (playerX < 0) playerX = 0;
    if (playerX > SCREEN_W - 8) playerX = SCREEN_W - 8;
    if (playerY < 0) playerY = 0;
    if (playerY > SCREEN_H - 8) playerY = SCREEN_H - 8;
}

/* ------------------------------------------------------------------------- */
/* Rendering                                                                 */
/* ------------------------------------------------------------------------- */
static void buildSpriteList(void)
{
    const u16 attrBullet = TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, TILE_BULLET);
    const u16 attrPlayer = TILE_ATTR_FULL(PAL2, TRUE, FALSE, FALSE, TILE_PLAYER);
    const u16 attrEnemy  = TILE_ATTR_FULL(PAL3, TRUE, FALSE, FALSE, TILE_ENEMY);
    u16 i;

    /* The link field is set to the next index; the last entry links to 0 (end) */
    VDP_setSpriteFull(SPR_PLAYER, playerX, playerY,
                      SPRITE_SIZE(1, 1), attrPlayer, SPR_PLAYER + 1);

    VDP_setSpriteFull(SPR_ENEMY, CENTER_X - 8, CENTER_Y - 8,
                      SPRITE_SIZE(2, 2), attrEnemy, SPR_ENEMY + 1);

    for (i = 0; i < MAX_BULLETS; i++)
    {
        const Bullet *b = &bullets[i];
        u16 idx  = SPR_BULLET0 + i;
        u8  link = (i == MAX_BULLETS - 1) ? 0 : (u8)(idx + 1);

        if (b->active)
            VDP_setSpriteFull(idx, (s16)(b->x >> 8) - 4, (s16)(b->y >> 8) - 4,
                              SPRITE_SIZE(1, 1), attrBullet, link);
        else
            VDP_setSpriteFull(idx, HIDDEN_X, HIDDEN_Y,
                              SPRITE_SIZE(1, 1), attrBullet, link);
    }
}

static void drawHud(void)
{
    static u16 lastCount = 0xFFFF;
    char buf[8];

    if (activeCount == lastCount) return;
    lastCount = activeCount;

    uintToStr(activeCount, buf, 2);
    VDP_drawText(buf, 9, 1);
}

/* ------------------------------------------------------------------------- */
/* Entry point                                                               */
/* ------------------------------------------------------------------------- */
int main(bool hardReset)
{
    u16 i;
    (void)hardReset;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    /* Palettes: PAL1 bullets, PAL2 player, PAL3 enemy; backdrop is color 0 of PAL0 */
    PAL_setColor(0, 0x0200);
    PAL_setColors(PAL1 * 16, bulletPal, 4, CPU);
    PAL_setColors(PAL2 * 16, playerPal, 4, CPU);
    PAL_setColors(PAL3 * 16, enemyPal,  5, CPU);

    /* Tiles */
    VDP_loadTileData(bulletTile,  TILE_BULLET, 1, CPU);
    VDP_loadTileData(playerTile,  TILE_PLAYER, 1, CPU);
    VDP_loadTileData(enemyTiles,  TILE_ENEMY,  4, CPU);

    /* Sprite list */
    VDP_resetSprites();

    /* State */
    for (i = 0; i < MAX_BULLETS; i++) bullets[i].active = 0;
    activeCount = 0;
    fireTimer   = 0;                    /* fires on the first frame */
    ringParity  = 0;
    playerX     = CENTER_X - 4;
    playerY     = SCREEN_H - 32;

    VDP_drawText("BULLETS", 1, 1);

    while (TRUE)
    {
        updatePlayer();

        if (fireTimer == 0)
        {
            spawnRing();
            fireTimer = FIRE_INTERVAL;
        }
        fireTimer--;

        updateBullets();
        buildSpriteList();
        drawHud();

        /* Whole 66-entry table goes to VRAM during vblank (queued DMA) */
        VDP_updateSprites(SPRITE_COUNT, DMA_QUEUE);
        SYS_doVBlankProcess();
    }

    return 0;
}
