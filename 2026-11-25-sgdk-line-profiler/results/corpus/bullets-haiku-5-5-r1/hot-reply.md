```c
#include <genesis.h>

/* ---------- Tunables ---------- */
#define MAX_BULLETS     40
#define FIRE_INTERVAL   10      /* frames between shots */
#define BULLET_SPEED    48      /* in 1/16 px per frame (48 = 3 px/frame) */
#define PLAYER_SPEED    2
#define ENEMY_SPEED     1

/* ---------- Hardware sprite slots ---------- */
#define SPR_PLAYER      0
#define SPR_ENEMY       1
#define SPR_BULLET_BASE 2
#define SPR_COUNT       (SPR_BULLET_BASE + MAX_BULLETS)

/* ---------- VRAM tile indices (loaded from index 1) ---------- */
#define TILE_DIGIT      1       /* 1..10 = digits '0'..'9' */
#define TILE_H          11
#define TILE_I          12
#define TILE_T          13
#define TILE_S          14
#define TILE_BULLET     15
#define TILE_SHIP       16      /* 16..19 = 2x2 ship, column-major order */
#define NUM_TILES       19

/* ---------- Palettes (format 0x0BGR, 3 bits per channel) ---------- */
static const u16 palette_main[16] = {
    0x0000, 0x0EEE, 0x000E, 0x00E0, 0x0E00, 0x00EE, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0EEE
};

static const u16 palette_enemy[16] = {
    0x0000, 0x0EEE, 0x0000, 0x000E, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0EEE
};

/*
 * Tile data: 8 u32 per 8x8 tile, one u32 per row, 8 nibbles = 8 pixels
 * (leftmost pixel in the high nibble). Nibble value = palette colour index,
 * 0 = transparent.
 *
 * Order: digits 0-9, 'H', 'I', 'T', 'S', bullet, ship TL, BL, TR, BR
 * Font tiles use colour 1 (white). Bullet uses colour 2 (red).
 * Ship uses colour 3 (green on PAL0, red on PAL1 for the enemy).
 */
static const u32 tile_data[NUM_TILES * 8] = {
    /* 0 */
    0x00111100, 0x01100110, 0x01100110, 0x01100110,
    0x01100110, 0x01100110, 0x00111100, 0x00000000,
    /* 1 */
    0x00011000, 0x00111000, 0x00011000, 0x00011000,
    0x00011000, 0x00011000, 0x00111100, 0x00000000,
    /* 2 */
    0x00111100, 0x01100110, 0x00000110, 0x00001100,
    0x00110000, 0x01100000, 0x01111110, 0x00000000,
    /* 3 */
    0x00111100, 0x01100110, 0x00000110, 0x00011100,
    0x00000110, 0x01100110, 0x00111100, 0x00000000,
    /* 4 */
    0x00001100, 0x00011100, 0x00111100, 0x01101100,
    0x01111110, 0x00001100, 0x00001100, 0x00000000,
    /* 5 */
    0x01111110, 0x01100000, 0x01111100, 0x00000110,
    0x00000110, 0x01100110, 0x00111100, 0x00000000,
    /* 6 */
    0x00011100, 0x00110000, 0x01100000, 0x01111100,
    0x01100110, 0x01100110, 0x00111100, 0x00000000,
    /* 7 */
    0x01111110, 0x01100110, 0x00000110, 0x00001100,
    0x00011000, 0x00011000, 0x00011000, 0x00000000,
    /* 8 */
    0x00111100, 0x01100110, 0x01100110, 0x00111100,
    0x01100110, 0x01100110, 0x00111100, 0x00000000,
    /* 9 */
    0x00111100, 0x01100110, 0x01100110, 0x00111110,
    0x00000110, 0x00001100, 0x00111000, 0x00000000,
    /* H (11) */
    0x01100110, 0x01100110, 0x01100110, 0x01111110,
    0x01100110, 0x01100110, 0x01100110, 0x00000000,
    /* I (12) */
    0x00111100, 0x00011000, 0x00011000, 0x00011000,
    0x00011000, 0x00011000, 0x00111100, 0x00000000,
    /* T (13) */
    0x01111110, 0x00011000, 0x00011000, 0x00011000,
    0x00011000, 0x00011000, 0x00011000, 0x00000000,
    /* S (14) */
    0x00111100, 0x01100110, 0x01100000, 0x00111100,
    0x00000110, 0x01100110, 0x00111100, 0x00000000,
    /* Bullet (15) */
    0x00222200, 0x02222220, 0x22222222, 0x22222222,
    0x22222222, 0x22222222, 0x02222220, 0x00222200,
    /* Ship top-left (16) */
    0x00000033, 0x00000333, 0x00003333, 0x00033333,
    0x00333333, 0x03333333, 0x33333333, 0x33333333,
    /* Ship bottom-left (17) */
    0x33333333, 0x33333333, 0x33333333, 0x33333333,
    0x00333333, 0x00033333, 0x00003333, 0x00000333,
    /* Ship top-right (18) */
    0x33000000, 0x33300000, 0x33330000, 0x33333000,
    0x33333300, 0x33333330, 0x33333333, 0x33333333,
    /* Ship bottom-right (19) */
    0x33333333, 0x33333333, 0x33333333, 0x33333333,
    0x33333300, 0x33333000, 0x33330000, 0x33300000
};

/* ---------- Game state ---------- */
typedef struct {
    s32 x, y;       /* centre position, 1/16 px */
    s32 vx, vy;     /* velocity, 1/16 px per frame */
    u8  active;
} Bullet;

static Bullet bullets[MAX_BULLETS];
static u8  activeList[MAX_BULLETS];   /* indices of active bullets (unordered) */
static u16 activeCount = 0;

static s32 absi(s32 v)
{
    return (v < 0) ? -v : v;
}

/* Move a bullet's hardware sprite off-screen (same values as before). */
static void parkBullet(u16 i)
{
    VDP_setSprite(SPR_BULLET_BASE + i, 0, 240, SPRITE_SIZE(1, 1),
                  TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_BULLET));
}

static void drawHits(u32 n)
{
    u8 started = FALSE;

    for (s16 i = 4; i >= 0; i--)
    {
        u16 d = (u16)(n % 10);
        u16 tile;
        n /= 10;

        if (d == 0 && !started && i > 0)
            tile = 0;                       /* blank leading zero */
        else
        {
            started = TRUE;
            tile = TILE_DIGIT + d;
        }

        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, tile), 6 + i, 1);
    }
}

int main(bool hardReset)
{
    (void)hardReset;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    JOY_init();

    PAL_setPalette(PAL0, palette_main, CPU);
    PAL_setPalette(PAL1, palette_enemy, CPU);
    VDP_loadTileData(tile_data, 1, NUM_TILES, CPU);

    VDP_clearPlane(BG_A, FALSE);

    /* "HITS" label */
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_H), 1, 1);
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_I), 2, 1);
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_T), 3, 1);
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_S), 4, 1);

    /* All bullet slots start parked; sprite cache persists between frames. */
    for (u16 i = 0; i < MAX_BULLETS; i++)
    {
        bullets[i].active = FALSE;
        parkBullet(i);
    }

    s32 px = 152, py = 176;     /* player top-left */
    s32 ex = 144, ey = 24;      /* enemy top-left  */
    s32 edir = 1;
    s32 prevPx = -1, prevPy = -1;   /* last written player position */
    u32 hits = 0;
    u8  needRedraw = TRUE;
    u16 frame = 0;
    u16 fireTimer = 0;          /* always equals frame % FIRE_INTERVAL */

    while (TRUE)
    {
        /* ----- Input ----- */
        u16 joy = JOY_readJoypad(JOY_1);

        if (joy & BUTTON_LEFT)  px -= PLAYER_SPEED;
        if (joy & BUTTON_RIGHT) px += PLAYER_SPEED;
        if (joy & BUTTON_UP)    py -= PLAYER_SPEED;
        if (joy & BUTTON_DOWN)  py += PLAYER_SPEED;

        if (px < 0)   px = 0;
        if (px > 304) px = 304;     /* 320 - 16 */
        if (py < 0)   py = 0;
        if (py > 208) py = 208;     /* 224 - 16 */

        /* ----- Enemy movement (bounces left/right) ----- */
        ex += edir * ENEMY_SPEED;
        if (ex <= 0)   { ex = 0;   edir = 1;  }
        if (ex >= 304) { ex = 304; edir = -1; }

        /* ----- Fire every FIRE_INTERVAL frames, aimed at player ----- */
        if (fireTimer == 0)
        {
            for (u16 i = 0; i < MAX_BULLETS; i++)
            {
                if (!bullets[i].active)
                {
                    s32 dx = px - ex;               /* (px+8) - (ex+8) */
                    s32 dy = py - ey - 8;           /* (py+8) - (ey+16) */
                    s32 dist = (absi(dx) > absi(dy)) ? absi(dx) : absi(dy);
                    if (dist == 0) dist = 1;

                    bullets[i].x  = (ex + 8)  << 4;
                    bullets[i].y  = (ey + 16) << 4;
                    bullets[i].vx = dx * BULLET_SPEED / dist;
                    bullets[i].vy = dy * BULLET_SPEED / dist;
                    bullets[i].active = TRUE;
                    activeList[activeCount++] = (u8)i;
                    break;
                }
            }
        }

        /* ----- Update active bullets, remove off-screen, check hits ----- */
        s32 pcx = px + 8;
        s32 pcy = py + 8;

        for (u16 k = 0; k < activeCount; )
        {
            u16 i = activeList[k];
            Bullet *b = &bullets[i];

            b->x += b->vx;
            b->y += b->vy;

            s32 bx = b->x >> 4;
            s32 by = b->y >> 4;

            u8 remove = FALSE;

            /* off-screen: bx < -8 || bx > 328 || by < -8 || by > 232 */
            if ((u32)(bx + 8) > 336u || (u32)(by + 8) > 240u)
            {
                remove = TRUE;
            }
            /* hit: |bx - pcx| < 6 && |by - pcy| < 6 */
            else if ((u32)(bx - pcx + 5) < 11u && (u32)(by - pcy + 5) < 11u)
            {
                hits++;
                needRedraw = TRUE;
                remove = TRUE;
            }

            if (remove)
            {
                b->active = FALSE;
                activeList[k] = activeList[--activeCount];   /* swap-in unprocessed entry */
                parkBullet(i);
            }
            else
            {
                k++;
            }
        }

        /* ----- Build sprite table (links are set automatically by SGDK) ----- */
        if (px != prevPx || py != prevPy)
        {
            VDP_setSprite(SPR_PLAYER, (s16)px, (s16)py,
                          SPRITE_SIZE(2, 2),
                          TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_SHIP));
            prevPx = px;
            prevPy = py;
        }

        VDP_setSprite(SPR_ENEMY, (s16)ex, (s16)ey,
                      SPRITE_SIZE(2, 2),
                      TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, TILE_SHIP));

        for (u16 k = 0; k < activeCount; k++)
        {
            u16 i = activeList[k];
            s16 sx = (s16)((bullets[i].x >> 4) - 4);
            s16 sy = (s16)((bullets[i].y >> 4) - 4);
            VDP_setSprite(SPR_BULLET_BASE + i, sx, sy, SPRITE_SIZE(1, 1),
                          TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_BULLET));
        }

        VDP_updateSprites(SPR_COUNT, DMA_QUEUE);

        /* ----- HUD ----- */
        if (needRedraw)
        {
            drawHits(hits);
            needRedraw = FALSE;
        }

        frame++;
        fireTimer++;
        if (fireTimer == FIRE_INTERVAL || frame == 0)   /* frame == 0: u16 wrap */
            fireTimer = 0;

        SYS_doVBlankProcess();
    }

    return 0;
}
```

Changes, all intended to leave game state and rendered output identical:

- **Bullet loops iterate only active bullets.** A compact list of active indices replaces the 40-slot scans in the update and sprite passes. Slot indices are unchanged, so sprite z-order is preserved. Bullets are independent, so processing order doesn't affect the result.
- **Parked sprites are written only on transition.** Each bullet is parked once when it deactivates, and all 40 slots are parked at startup. The sprite cache persists between frames, so the VRAM sprite table ends up the same.
- **Player sprite is written only when it moves.**
- **No divide per frame.** `frame % FIRE_INTERVAL` is replaced by a counter that resets on the u16 wrap, so the firing pattern matches the original exactly.
- **Cheaper range checks.** The bounds and hit tests use single unsigned compares with the same boundaries.

`SYS_doVBlankProcess` is SGDK's fixed VBlank and DMA work, so it stays the same. The remaining gains come from the game-side loops and sprite calls.