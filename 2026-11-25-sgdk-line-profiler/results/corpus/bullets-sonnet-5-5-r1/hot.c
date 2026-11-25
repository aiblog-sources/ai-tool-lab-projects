#include <genesis.h>

#define MAX_BULLETS      40
#define FIRE_INTERVAL    10
#define BULLET_SPEED     320      /* 8.8 fixed point (1.25 px/frame) */
#define PLAYER_SPEED     2

#define TILE_BULLET      (TILE_USER_INDEX + 0)
#define TILE_PLAYER      (TILE_USER_INDEX + 1)
#define TILE_ENEMY       (TILE_USER_INDEX + 2)   /* 4 tiles, 2x2 sprite */

/* Sprite slots: player in front, then enemy, then bullets */
#define SPR_PLAYER       0
#define SPR_ENEMY        1
#define SPR_BULLET0      2
#define NUM_SPRITES      (SPR_BULLET0 + MAX_BULLETS)

#define ENEMY_Y          16
#define ENEMY_MIN_X      8
#define ENEMY_MAX_X      296

/* Tile data: 8 u32 per 8x8 tile, one nibble per pixel (palette index) */
static const u32 tileData[6 * 8] =
{
    /* Bullet */
    0x00111100,
    0x01122110,
    0x11222211,
    0x12222221,
    0x12222221,
    0x11222211,
    0x01122110,
    0x00111100,

    /* Player */
    0x00033000,
    0x00033000,
    0x00344300,
    0x00344300,
    0x03344330,
    0x03333330,
    0x33000033,
    0x30000003,

    /* Enemy: top-left */
    0x00000555,
    0x00055555,
    0x00555555,
    0x05555555,
    0x05544555,
    0x05544555,
    0x05555555,
    0x05555555,

    /* Enemy: bottom-left */
    0x05555555,
    0x05555555,
    0x00555555,
    0x00555550,
    0x00055500,
    0x00005500,
    0x00000500,
    0x00000000,

    /* Enemy: top-right */
    0x55500000,
    0x55555000,
    0x55555500,
    0x55555550,
    0x55544550,
    0x55544550,
    0x55555550,
    0x55555550,

    /* Enemy: bottom-right */
    0x55555550,
    0x55555550,
    0x55555500,
    0x05555500,
    0x00555000,
    0x00550000,
    0x00500000,
    0x00000000
};

typedef struct
{
    s32 x, y;      /* 16.16 fixed point, top-left of the 8x8 sprite (integer part = high word) */
    s32 vx, vy;    /* 16.16 fixed point */
    u8  active;
} Bullet;

static Bullet bullets[MAX_BULLETS];
static u16 activeCount;

static s16 playerX, playerY;
static s16 enemyX;
static s16 enemyDir;
static u16 hits;
static u16 frameCount;

/* Integer part of a 16.16 value (68000 is big-endian: high word first) */
#define INT_PART(v)  (*(s16 *)&(v))

static void drawHits(void)
{
    char buf[8];
    intToStr(hits, buf, 5);
    VDP_drawText("HITS:", 1, 1);
    VDP_drawText(buf, 7, 1);
}

static void fireBullet(void)
{
    u16 i;
    for (i = 0; i < MAX_BULLETS; i++)
    {
        if (!bullets[i].active)
        {
            /* Aim from enemy center to player center */
            s32 sx = enemyX + 8;
            s32 sy = ENEMY_Y + 8;
            s32 dx = (playerX + 4) - sx;
            s32 dy = (playerY + 4) - sy;
            s32 adx = dx < 0 ? -dx : dx;
            s32 ady = dy < 0 ? -dy : dy;
            s32 len = (adx > ady) ? (adx + (ady >> 1)) : (ady + (adx >> 1));
            if (len == 0) len = 1;

            bullets[i].x  = (sx - 4) << 16;
            bullets[i].y  = (sy - 4) << 16;
            bullets[i].vx = ((dx * BULLET_SPEED) / len) << 8;
            bullets[i].vy = ((dy * BULLET_SPEED) / len) << 8;
            bullets[i].active = TRUE;
            activeCount++;
            return;
        }
    }
    /* All 40 slots in use: skip this shot */
}

int main(bool hardReset)
{
    u16 i;

    VDP_setScreenWidth320();
    VDP_setBackgroundColor(0);

    /* Palette 1 for sprites */
    PAL_setColor(0, 0x0200);            /* background (dark blue-ish) */
    PAL_setColor(16 + 1, 0x000E);       /* red */
    PAL_setColor(16 + 2, 0x00EE);       /* yellow */
    PAL_setColor(16 + 3, 0x0EE0);       /* cyan */
    PAL_setColor(16 + 4, 0x0EEE);       /* white */
    PAL_setColor(16 + 5, 0x0A0A);       /* purple */

    VDP_loadTileData(tileData, TILE_USER_INDEX, 6, CPU);

    /* Init state */
    playerX = 156;
    playerY = 180;
    enemyX = 152;
    enemyDir = 1;
    hits = 0;
    frameCount = 0;
    activeCount = 0;
    for (i = 0; i < MAX_BULLETS; i++) bullets[i].active = FALSE;

    /* Init sprites (raw VDP coordinates have a 128 pixel offset) */
    VDP_resetSprites();
    VDP_setSprite(SPR_PLAYER, playerX + 0x80, playerY + 0x80, SPRITE_SIZE(1, 1),
                  TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, TILE_PLAYER));
    VDP_setSprite(SPR_ENEMY, enemyX + 0x80, ENEMY_Y + 0x80, SPRITE_SIZE(2, 2),
                  TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, TILE_ENEMY));
    for (i = 0; i < MAX_BULLETS; i++)
    {
        VDP_setSprite(SPR_BULLET0 + i, 0, 0, SPRITE_SIZE(1, 1),
                      TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, TILE_BULLET));
    }
    VDP_linkSprites(0, NUM_SPRITES);

    drawHits();

    while (TRUE)
    {
        u16 joy = JOY_readJoypad(JOY_1);
        u16 oldHits = hits;
        s16 px = playerX;
        s16 py = playerY;

        /* Player movement */
        if (joy & BUTTON_LEFT)  px -= PLAYER_SPEED;
        if (joy & BUTTON_RIGHT) px += PLAYER_SPEED;
        if (joy & BUTTON_UP)    py -= PLAYER_SPEED;
        if (joy & BUTTON_DOWN)  py += PLAYER_SPEED;
        if (px < 0)   px = 0;
        if (px > 312) px = 312;
        if (py < 32)  py = 32;
        if (py > 216) py = 216;
        playerX = px;
        playerY = py;

        /* Enemy patrol */
        enemyX += enemyDir;
        if (enemyX <= ENEMY_MIN_X) { enemyX = ENEMY_MIN_X; enemyDir = 1; }
        if (enemyX >= ENEMY_MAX_X) { enemyX = ENEMY_MAX_X; enemyDir = -1; }

        /* Fire every 10 frames */
        frameCount++;
        if (frameCount >= FIRE_INTERVAL)
        {
            frameCount = 0;
            fireBullet();
        }

        /* Update bullets */
        {
            u16 left = activeCount;
            Bullet *b = bullets;
            VDPSprite *sp = &vdpSpriteCache[SPR_BULLET0];

            for (; left; b++, sp++)
            {
                if (!b->active) continue;
                left--;

                b->x += b->vx;
                b->y += b->vy;

                s16 bx = INT_PART(b->x);
                s16 by = INT_PART(b->y);

                /* off-screen: bx < -8 || bx > 320 || by < -8 || by > 224 */
                if ((u16)(bx + 8) > 328 || (u16)(by + 8) > 232)
                {
                    b->active = FALSE;
                    activeCount--;
                    sp->x = 0;
                    sp->y = 0;
                    continue;
                }

                /* Hit test: centers within 5 px on both axes (|d| < 5) */
                if ((u16)(bx - px + 4) < 9 && (u16)(by - py + 4) < 9)
                {
                    hits++;
                    b->active = FALSE;
                    activeCount--;
                    sp->x = 0;
                    sp->y = 0;
                    continue;
                }

                sp->x = bx + 0x80;
                sp->y = by + 0x80;
            }
        }

        VDP_setSpritePosition(SPR_PLAYER, px + 0x80, py + 0x80);
        VDP_setSpritePosition(SPR_ENEMY, enemyX + 0x80, ENEMY_Y + 0x80);

        if (hits != oldHits) drawHits();

        VDP_updateSprites(NUM_SPRITES, DMA_QUEUE);
        SYS_doVBlankProcess();
    }

    return 0;
}
