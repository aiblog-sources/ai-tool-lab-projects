Here's the optimized `main.c`. Gameplay and visuals should be unchanged. I haven't built it or run it on hardware or in the emulator.

- **Sprite table:** the bullet sprites' size, link and tile attributes never change, so I write them once at startup. Each frame only writes y and x, directly into `vdpSpriteCache`, instead of calling `VDP_setSpriteFull` 42 times.
- **One bullet loop:** movement, bounds check, collision and the sprite write now happen in a single pass. Inactive bullets are skipped, and their sprite entry stays at (0,0) from init. A bullet that dies writes (0,0) once, which is what the old code produced.
- **Fixed point:** bullet positions are stored as 16.16 instead of 8.8, with the velocity pre-shifted left by 8. The pixel position is then `x >> 16`, which the compiler can do with a `swap` instead of eight shifts. The arithmetic is otherwise the same.
- **Range checks:** the four-compare bounds test and the two `abs` calls in the collision test become unsigned range checks, which are cheaper on the 68000.

```c
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
    s32 x;      /* 16.16 fixed point (pixel in high word) */
    s32 y;
    s32 vx;     /* 8.8 velocity pre-shifted to 16.16 */
    s32 vy;
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

            bullets[i].x = ((s32) (sx - 4)) << 16;
            bullets[i].y = ((s32) (sy - 4)) << 16;
            bullets[i].vx = ((s32) (s16) (((s32) dx * BULLET_SPEED) / dist)) << 8;
            bullets[i].vy = ((s32) (s16) (((s32) dy * BULLET_SPEED) / dist)) << 8;
            bullets[i].active = TRUE;
            return;
        }
    }
}

static void drawHits(void)
{
    char buf[12];
    intToStr(hits, buf, 4);
    VDP_drawText("HITS:", 1, 1);
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

    /* static sprite fields, written once */
    {
        VDPSprite *spr = &vdpSpriteCache[SPR_ENEMY];
        spr->size_link = (SPRITE_SIZE(2, 2) << 8) | SPR_PLAYER;
        spr->attribut = TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, TILE_ENEMY);

        spr = &vdpSpriteCache[SPR_PLAYER];
        spr->size_link = (SPRITE_SIZE(1, 1) << 8) | SPR_BULLET0;
        spr->attribut = TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, TILE_PLAYER);

        for (i = 0; i < MAX_BULLETS; i++)
        {
            spr = &vdpSpriteCache[SPR_BULLET0 + i];
            spr->y = 0;
            spr->x = 0;
            spr->size_link = (SPRITE_SIZE(1, 1) << 8) |
                             ((i == MAX_BULLETS - 1) ? 0 : (SPR_BULLET0 + i + 1));
            spr->attribut = TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, TILE_BULLET);
        }
    }

    drawHits();

    while (TRUE)
    {
        u16 joy = JOY_readJoypad(JOY_1);
        Bullet *b;
        VDPSprite *spr;
        s16 px, py;

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

        px = playerX;
        py = playerY;

        /* update bullets and their sprite entries in one pass */
        b = bullets;
        spr = &vdpSpriteCache[SPR_BULLET0];
        for (i = MAX_BULLETS; i != 0; i--, b++, spr++)
        {
            s16 bx, by, ddx, ddy;

            if (!b->active) continue;

            b->x += b->vx;
            b->y += b->vy;
            bx = (s16) (b->x >> 16);
            by = (s16) (b->y >> 16);

            /* off-screen: bx < -8 || bx > 320 || by < -8 || by > 224 */
            if ((u16) (bx + 8) > 328 || (u16) (by + 8) > 232)
            {
                b->active = FALSE;
                spr->x = 0;
                spr->y = 0;
                continue;
            }

            /* collision: |dx| < 5 && |dy| < 5 */
            ddx = bx - px;
            ddy = by - py;
            if ((u16) (ddx + 4) < 9 && (u16) (ddy + 4) < 9)
            {
                b->active = FALSE;
                hits++;
                spr->x = 0;
                spr->y = 0;
                continue;
            }

            spr->x = bx + 0x80;
            spr->y = by + 0x80;
        }

        /* enemy and player sprites (raw coordinates need +0x80 offset) */
        vdpSpriteCache[SPR_ENEMY].y = enemyY + 0x80;
        vdpSpriteCache[SPR_ENEMY].x = enemyX + 0x80;
        vdpSpriteCache[SPR_PLAYER].y = py + 0x80;
        vdpSpriteCache[SPR_PLAYER].x = px + 0x80;

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
```

The `SYS_doVBlankProcess` line (about 4,300 cycles) is SGDK's own VBlank work, including the sprite DMA, so I left it alone. If your SGDK build's `VDPSprite` has no `size_link` field, tell me and I'll switch to the separate size and link fields.