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

/* Positions/velocities are 16.16 fixed point so the integer pixel is
   simply the high word (cheap to extract on the 68000). The values are
   exactly the old 24.8 numbers << 8, so behaviour is identical. */
typedef struct
{
    s32 x, y;
    s32 vx, vy;
    u8  active;
} Bullet;

static Bullet bullets[MAX_BULLETS];

static s16 playerX, playerY;
static s16 enemyX;
static s16 enemyDir;
static u16 hits;
static u16 frameCount;

static void drawHits(void)
{
    char buf[8];
    intToStr(hits, buf, 5);
    VDP_drawText("HITS:", 1, 1);
    VDP_drawText(buf, 7, 1);
}

/* 32/16 -> 16 signed divide using the native DIVS instruction.
   Quotient is guaranteed to fit in 16 bits here (|q| <= 320). */
static inline s16 divs16(s32 n, s16 d)
{
    s32 r = n;
    __asm__("divs.w %1,%0" : "+d"(r) : "dm"(d) : "cc");
    return (s16)r;
}

static void fireBullet(void)
{
    u16 i;
    for (i = 0; i < MAX_BULLETS; i++)
    {
        if (!bullets[i].active)
        {
            /* Aim from enemy center to player center */
            s16 sx = enemyX + 8;
            s16 sy = ENEMY_Y + 8;
            s16 dx = (playerX + 4) - sx;
            s16 dy = (playerY + 4) - sy;
            s16 adx = dx < 0 ? -dx : dx;
            s16 ady = dy < 0 ? -dy : dy;
            s16 len = (adx > ady) ? (adx + (ady >> 1)) : (ady + (adx >> 1));
            if (len == 0) len = 1;

            bullets[i].x  = ((s32)(sx - 4)) << 16;
            bullets[i].y  = ((s32)(sy - 4)) << 16;
            bullets[i].vx = ((s32)divs16((s32)dx * BULLET_SPEED, len)) << 8;
            bullets[i].vy = ((s32)divs16((s32)dy * BULLET_SPEED, len)) << 8;
            bullets[i].active = TRUE;
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

        /* Player movement */
        if (joy & BUTTON_LEFT)  playerX -= PLAYER_SPEED;
        if (joy & BUTTON_RIGHT) playerX += PLAYER_SPEED;
        if (joy & BUTTON_UP)    playerY -= PLAYER_SPEED;
        if (joy & BUTTON_DOWN)  playerY += PLAYER_SPEED;
        if (playerX < 0)   playerX = 0;
        if (playerX > 312) playerX = 312;
        if (playerY < 32)  playerY = 32;
        if (playerY > 216) playerY = 216;

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
            const s16 px = playerX;
            const s16 py = playerY;
            Bullet *b = bullets;
            VDPSprite *spr = &vdpSpriteCache[SPR_BULLET0];

            for (i = MAX_BULLETS; i != 0; i--, b++, spr++)
            {
                if (!b->active) continue;

                b->x += b->vx;
                b->y += b->vy;

                s16 bx = (s16)(b->x >> 16);
                s16 by = (s16)(b->y >> 16);

                /* bx < -8 || bx > 320 || by < -8 || by > 224 */
                if ((u16)(bx + 8) > 328 || (u16)(by + 8) > 232)
                {
                    b->active = FALSE;
                    spr->x = 0;
                    spr->y = 0;
                    continue;
                }

                /* Hit test: centers within 5 px on both axes */
                s16 dx = bx - px;
                s16 dy = by - py;
                if (dx < 0) dx = -dx;
                if (dy < 0) dy = -dy;
                if (dx < 5 && dy < 5)
                {
                    hits++;
                    b->active = FALSE;
                    spr->x = 0;
                    spr->y = 0;
                    continue;
                }

                spr->x = bx + 0x80;
                spr->y = by + 0x80;
            }
        }

        vdpSpriteCache[SPR_PLAYER].x = playerX + 0x80;
        vdpSpriteCache[SPR_PLAYER].y = playerY + 0x80;
        vdpSpriteCache[SPR_ENEMY].x  = enemyX + 0x80;
        vdpSpriteCache[SPR_ENEMY].y  = ENEMY_Y + 0x80;

        if (hits != oldHits) drawHits();

        VDP_updateSprites(NUM_SPRITES, DMA_QUEUE);
        SYS_doVBlankProcess();
    }

    return 0;
}
