#include <genesis.h>

#define MAX_BULLETS     40
#define FIRE_INTERVAL   10
#define BULLET_SPEED    256     /* 1.0 px/frame in 8.8 fixed point */
#define PLAYER_SPEED    2

#define SPR_ENEMY       0
#define SPR_PLAYER      1
#define SPR_BULLET0     2
#define SPR_TOTAL       (2 + MAX_BULLETS)

#define TILE_BULLET     (TILE_USER_INDEX + 0)
#define TILE_PLAYER     (TILE_USER_INDEX + 1)
#define TILE_ENEMY      (TILE_USER_INDEX + 2)   /* 4 tiles, column-major for a 2x2 sprite */

/* Bullet: 8x8 round shot */
static const u32 tileBullet[8] =
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

/* Player: 8x8 diamond with bright core */
static const u32 tilePlayer[8] =
{
    0x00044000,
    0x00455400,
    0x04555540,
    0x45566554,
    0x45566554,
    0x04555540,
    0x00455400,
    0x00044000
};

/* Enemy: 16x16 made of 4 tiles, order: TL, BL, TR, BR */
static const u32 tileEnemy[4 * 8] =
{
    /* top-left */
    0x00777777,
    0x07777777,
    0x77777777,
    0x77711777,
    0x77711777,
    0x77777777,
    0x07777777,
    0x00777777,
    /* bottom-left */
    0x77777777,
    0x77777777,
    0x77788888,
    0x77777777,
    0x07777777,
    0x00777777,
    0x00077777,
    0x00007777,
    /* top-right */
    0x77777700,
    0x77777770,
    0x77777777,
    0x77711777,
    0x77711777,
    0x77777777,
    0x77777770,
    0x77777700,
    /* bottom-right */
    0x77777777,
    0x77777777,
    0x88888777,
    0x77777777,
    0x77777770,
    0x77777700,
    0x77777000,
    0x77770000
};

static const u16 spritePalette[16] =
{
    0x0000, /* 0 transparent */
    0x000E, /* 1 red */
    0x004E, /* 2 orange */
    0x00EE, /* 3 yellow */
    0x0E80, /* 4 blue */
    0x0EE0, /* 5 cyan */
    0x0EEE, /* 6 white */
    0x0A0A, /* 7 purple */
    0x0E0E, /* 8 magenta */
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

typedef struct
{
    s32 x;      /* 8.8 fixed point */
    s32 y;
    s16 vx;     /* 8.8 fixed point */
    s16 vy;
    u8 active;
} Bullet;

static Bullet bullets[MAX_BULLETS];

static s16 playerX, playerY;
static s16 enemyX, enemyY;
static s16 enemyDir;
static u16 hits;
static u16 frameCount;

static void fireBullet(void)
{
    u16 i;
    for (i = 0; i < MAX_BULLETS; i++)
    {
        if (!bullets[i].active)
        {
            /* centers */
            s16 sx = enemyX + 8;
            s16 sy = enemyY + 8;
            s16 dx = (playerX + 4) - sx;
            s16 dy = (playerY + 4) - sy;
            s16 ax = (dx < 0) ? -dx : dx;
            s16 ay = (dy < 0) ? -dy : dy;
            s16 dist;

            /* cheap distance approximation (no sqrt) */
            if (ax > ay) dist = ax + (ay >> 1);
            else dist = ay + (ax >> 1);
            if (dist == 0)
            {
                dx = 0;
                dy = 1;
                dist = 1;
            }

            bullets[i].x = ((s32) (sx - 4)) << 8;
            bullets[i].y = ((s32) (sy - 4)) << 8;
            /* hardware DIVS: quotient magnitude <= 256, fits in s16 */
            bullets[i].vx = divs((s32) dx * BULLET_SPEED, dist);
            bullets[i].vy = divs((s32) dy * BULLET_SPEED, dist);
            bullets[i].active = TRUE;
            return;
        }
    }
}

/* Only the number is redrawn; the "HITS:" label is drawn once at startup */
static void drawHits(void)
{
    char buf[12];
    intToStr(hits, buf, 4);
    VDP_drawText(buf, 7, 1);
}

int main(bool hardReset)
{
    u16 i;
    u16 lastHits;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();
    VDP_setBackgroundColor(0);

    PAL_setPalette(PAL1, spritePalette, CPU);
    PAL_setColor(15, 0x0EEE);   /* white text */

    VDP_loadTileData(tileBullet, TILE_BULLET, 1, CPU);
    VDP_loadTileData(tilePlayer, TILE_PLAYER, 1, CPU);
    VDP_loadTileData(tileEnemy, TILE_ENEMY, 4, CPU);

    VDP_resetSprites();

    playerX = 156;
    playerY = 180;
    enemyX = 152;
    enemyY = 24;
    enemyDir = 1;
    hits = 0;
    lastHits = 0;
    frameCount = 0;

    for (i = 0; i < MAX_BULLETS; i++) bullets[i].active = FALSE;

    VDP_drawText("HITS:", 1, 1);
    drawHits();

    while (TRUE)
    {
        u16 joy = JOY_readJoypad(JOY_1);

        /* player movement */
        if (joy & BUTTON_LEFT)  playerX -= PLAYER_SPEED;
        if (joy & BUTTON_RIGHT) playerX += PLAYER_SPEED;
        if (joy & BUTTON_UP)    playerY -= PLAYER_SPEED;
        if (joy & BUTTON_DOWN)  playerY += PLAYER_SPEED;

        if (playerX < 0) playerX = 0;
        if (playerX > 312) playerX = 312;
        if (playerY < 48) playerY = 48;
        if (playerY > 216) playerY = 216;

        /* enemy movement */
        enemyX += enemyDir;
        if (enemyX < 16) { enemyX = 16; enemyDir = 1; }
        if (enemyX > 288) { enemyX = 288; enemyDir = -1; }

        /* fire every FIRE_INTERVAL frames */
        frameCount++;
        if (frameCount >= FIRE_INTERVAL)
        {
            frameCount = 0;
            fireBullet();
        }

        /* update bullets */
        for (i = 0; i < MAX_BULLETS; i++)
        {
            Bullet *b = &bullets[i];
            s16 bx, by, ddx, ddy;

            if (!b->active) continue;

            b->x += b->vx;
            b->y += b->vy;
            bx = (s16) (b->x >> 8);
            by = (s16) (b->y >> 8);

            if (bx < -8 || bx > 320 || by < -8 || by > 224)
            {
                b->active = FALSE;
                continue;
            }

            /* collision: compare 8x8 centers, small hitbox */
            ddx = bx - playerX;
            ddy = by - playerY;
            if (ddx < 0) ddx = -ddx;
            if (ddy < 0) ddy = -ddy;
            if (ddx < 5 && ddy < 5)
            {
                b->active = FALSE;
                hits++;
            }
        }

        /* build sprite list (raw coordinates need +0x80 offset) */
        VDP_setSpriteFull(SPR_ENEMY, enemyX + 0x80, enemyY + 0x80,
                          SPRITE_SIZE(2, 2),
                          TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, TILE_ENEMY),
                          SPR_PLAYER);

        VDP_setSpriteFull(SPR_PLAYER, playerX + 0x80, playerY + 0x80,
                          SPRITE_SIZE(1, 1),
                          TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, TILE_PLAYER),
                          SPR_BULLET0);

        for (i = 0; i < MAX_BULLETS; i++)
        {
            s16 sx = 0, sy = 0;   /* raw 0 = off-screen */
            u8 link = (i == MAX_BULLETS - 1) ? 0 : (SPR_BULLET0 + i + 1);

            if (bullets[i].active)
            {
                sx = (s16) (bullets[i].x >> 8) + 0x80;
                sy = (s16) (bullets[i].y >> 8) + 0x80;
            }

            VDP_setSpriteFull(SPR_BULLET0 + i, sx, sy,
                              SPRITE_SIZE(1, 1),
                              TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, TILE_BULLET),
                              link);
        }

        VDP_updateSprites(SPR_TOTAL, DMA_QUEUE);

        if (hits != lastHits)
        {
            lastHits = hits;
            drawHits();
        }

        SYS_doVBlankProcess();
    }

    return 0;
}
