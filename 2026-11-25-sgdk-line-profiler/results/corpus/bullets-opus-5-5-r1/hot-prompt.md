Here is a Mega Drive game written with SGDK 2.09. Make it run faster on the real hardware (68000 at 7.67 MHz) without changing what the game does or how it looks. Reply with the full main.c in one ```c code block.

Below is a line profile of this code, measured in an emulator with a scripted pad: cycles per video frame by source line (one NTSC frame is 128,005 cycles), what each line called, and the findings of a performance lint on the same lines.

== Lines by measured cost (sgdkhot: mdlines + sgdklint) ==
budget 128,005 cycles per video frame (NTSC); 1,200 frames counted; busy 15,747 cycles per frame on average (12.3%)
each line = its own instructions + every library/libgcc call made from it (game helpers count on their own lines)
    3,777 /frame  24.0% of busy  src/main.c:299  SYS_doVBlankProcess();
                     3,740 in SYS_doVBlankProcessEx.constprop.0.isra.0: 1.00 calls/frame, 3,740 cycles each
    1,872 /frame  11.9% of busy  src/main.c:234  VDP_setSpriteFull(idx, (s16)(b->x >> FP_SHIFT), (s16)(b->y >
    1,477 /frame   9.4% of busy  src/main.c:241  VDP_updateSprites(idx, DMA_QUEUE);
                     1,294 in DMA_transfer.isra.0: 1.00 calls/frame, 1,294 cycles each
      994 /frame   6.3% of busy  src/main.c:174  for (u16 i = 0; i < MAX_BULLETS; i++)
      975 /frame   6.2% of busy  src/main.c:230  for (u16 i = 0; i < MAX_BULLETS; i++)
      873 /frame   5.5% of busy  src/main.c:177  if (!b->active) continue;
      872 /frame   5.5% of busy  src/main.c:233  if (!b->active) continue;
      545 /frame   3.5% of busy  src/main.c:185  if (bx < -8 || bx > SCREEN_W || by < -8 || by > SCREEN_H)
      481 /frame   3.1% of busy  src/main.c:180  b->y += b->vy;
      404 /frame   2.6% of busy  src/main.c:179  b->x += b->vx;
      402 /frame   2.6% of busy  src/main.c:194  if (ddx < 0) ddx = -ddx;
      349 /frame   2.2% of busy  src/main.c:195  if (ddy < 0) ddy = -ddy;
      318 /frame   2.0% of busy  [no game line]:0  
                       125 in no_bmp_task
                       123 in no_user_task
                        31 in no_xgm_task
      233 /frame   1.5% of busy  src/main.c:224  VDP_setSpriteFull(idx, enemyX, enemyY, SPRITE_SIZE(2, 2),
      230 /frame   1.5% of busy  src/main.c:182  s16 bx = (s16)(b->x >> FP_SHIFT);

== Lint findings, by measured cost ==
negligible        69 /frame (0.05% of a frame)  src/main.c:129 [div] guarded
negligible        68 /frame (0.05% of a frame)  src/main.c:130 [div] guarded
negligible        60 /frame (0.05% of a frame)  src/main.c:95 [text] guarded
negligible        21 /frame (0.02% of a frame)  src/main.c:94 [text] guarded
verdicts: matters >= 1% of a frame, small >= 0.2%, negligible below, not run = no cycles on that line in this run

== Game lines over 2% of a frame with no lint finding ==
    3,777 /frame  src/main.c:299 (main), mostly SYS_doVBlankProcessEx.constprop.0.isra.0

library code with no game line on the stack (interrupt handlers, SGDK's VBlank work): 318 cycles per frame

main.c:
// main.c - Bullet pattern dodge game for SGDK 2.09
// The enemy at the top fires an aimed bullet every 10 frames (max 40 on screen).
// Dodge with the D-pad on pad 1. The hit counter is shown at the top left.

#include <genesis.h>

#define MAX_BULLETS     40
#define FIRE_INTERVAL   10
#define FP_SHIFT        8                   // 8.8 fixed point
#define BULLET_SPEED    640                 // 2.5 px/frame in 8.8
#define PLAYER_SPEED    2
#define SCREEN_W        320
#define SCREEN_H        224

// Tile indices in VRAM
#define TILE_PLAYER     (TILE_USER_INDEX)
#define TILE_ENEMY      (TILE_USER_INDEX + 4)
#define TILE_BULLET     (TILE_USER_INDEX + 8)

// ---------------------------------------------------------------------------
// Tile data (4bpp, one u32 per 8-pixel row).
// 16x16 sprites use column-major tile order: TL, BL, TR, BR.
// ---------------------------------------------------------------------------
static const u32 playerTiles[4 * 8] =
{
    // top-left
    0x00000001, 0x00000011, 0x00000012, 0x00000112,
    0x00000111, 0x00001111, 0x00011111, 0x00111113,
    // bottom-left
    0x01111113, 0x11111111, 0x11110111, 0x11100011,
    0x11000001, 0x00000001, 0x00000011, 0x00000000,
    // top-right
    0x10000000, 0x11000000, 0x21000000, 0x21100000,
    0x11100000, 0x11110000, 0x11111000, 0x31111100,
    // bottom-right
    0x31111110, 0x11111111, 0x11101111, 0x11000111,
    0x10000011, 0x10000000, 0x11000000, 0x00000000
};

static const u32 enemyTiles[4 * 8] =
{
    // top-left
    0x00000000, 0x00770000, 0x07777000, 0x07777777,
    0x07788777, 0x77788777, 0x77777777, 0x77777778,
    // bottom-left
    0x07777788, 0x00777778, 0x00077777, 0x00007777,
    0x00000777, 0x00000077, 0x00000007, 0x00000000,
    // top-right
    0x00000000, 0x00007700, 0x00077770, 0x77777770,
    0x77788770, 0x77788777, 0x77777777, 0x87777777,
    // bottom-right
    0x88777770, 0x87777700, 0x77777000, 0x77770000,
    0x77700000, 0x77000000, 0x70000000, 0x00000000
};

static const u32 bulletTile[8] =
{
    0x00444400,
    0x04555540,
    0x45566554,
    0x45666654,
    0x45666654,
    0x45566554,
    0x04555540,
    0x00444400
};

// ---------------------------------------------------------------------------
// Game state
// ---------------------------------------------------------------------------
typedef struct
{
    s32 x, y;       // top-left, 8.8 fixed point
    s32 vx, vy;     // velocity, 8.8 fixed point
    u16 active;
} Bullet;

static Bullet bullets[MAX_BULLETS];
static u16 bulletCount;

static s16 playerX, playerY;
static s16 enemyX, enemyY;
static s16 enemyDir;

static u16 hits;
static u16 hitFlash;
static u16 fireTimer;
static u32 frameCounter;

// ---------------------------------------------------------------------------
static void drawHits(void)
{
    char buf[8];
    uintToStr(hits, buf, 5);
    VDP_drawText(buf, 7, 1);
}

static void fireBullet(void)
{
    if (bulletCount >= MAX_BULLETS) return;

    u16 i;
    for (i = 0; i < MAX_BULLETS; i++)
        if (!bullets[i].active) break;
    if (i == MAX_BULLETS) return;

    // Centers
    s16 ex = enemyX + 8;
    s16 ey = enemyY + 12;
    s16 px = playerX + 8;
    s16 py = playerY + 8;

    s32 dx = px - ex;
    s32 dy = py - ey;
    s32 adx = (dx < 0) ? -dx : dx;
    s32 ady = (dy < 0) ? -dy : dy;

    // Fast length approximation: max + 3/8 * min (no sqrt)
    s32 len = (adx > ady) ? (adx + ((ady * 3) >> 3)) : (ady + ((adx * 3) >> 3));

    Bullet *b = &bullets[i];
    if (len == 0)
    {
        b->vx = 0;
        b->vy = BULLET_SPEED;
    }
    else
    {
        b->vx = (dx * BULLET_SPEED) / len;
        b->vy = (dy * BULLET_SPEED) / len;
    }

    b->x = (s32)(ex - 4) << FP_SHIFT;
    b->y = (s32)(ey - 4) << FP_SHIFT;
    b->active = TRUE;
    bulletCount++;
}

static void updatePlayer(void)
{
    u16 pad = JOY_readJoypad(JOY_1);

    if (pad & BUTTON_LEFT)  playerX -= PLAYER_SPEED;
    if (pad & BUTTON_RIGHT) playerX += PLAYER_SPEED;
    if (pad & BUTTON_UP)    playerY -= PLAYER_SPEED;
    if (pad & BUTTON_DOWN)  playerY += PLAYER_SPEED;

    if (playerX < 0) playerX = 0;
    if (playerX > SCREEN_W - 16) playerX = SCREEN_W - 16;
    if (playerY < 48) playerY = 48;
    if (playerY > SCREEN_H - 16) playerY = SCREEN_H - 16;
}

static void updateEnemy(void)
{
    enemyX += enemyDir;
    if (enemyX <= 16)               { enemyX = 16; enemyDir = 1; }
    if (enemyX >= SCREEN_W - 32)    { enemyX = SCREEN_W - 32; enemyDir = -1; }

    if (++fireTimer >= FIRE_INTERVAL)
    {
        fireTimer = 0;
        fireBullet();
    }
}

static void updateBullets(void)
{
    // Player hitbox center (small, forgiving hitbox)
    s16 pcx = playerX + 8;
    s16 pcy = playerY + 9;
    u16 hitThisFrame = FALSE;

    for (u16 i = 0; i < MAX_BULLETS; i++)
    {
        Bullet *b = &bullets[i];
        if (!b->active) continue;

        b->x += b->vx;
        b->y += b->vy;

        s16 bx = (s16)(b->x >> FP_SHIFT);
        s16 by = (s16)(b->y >> FP_SHIFT);

        if (bx < -8 || bx > SCREEN_W || by < -8 || by > SCREEN_H)
        {
            b->active = FALSE;
            bulletCount--;
            continue;
        }

        s16 ddx = (bx + 4) - pcx;
        s16 ddy = (by + 4) - pcy;
        if (ddx < 0) ddx = -ddx;
        if (ddy < 0) ddy = -ddy;

        if (ddx < 6 && ddy < 6)
        {
            b->active = FALSE;
            bulletCount--;
            hits++;
            hitThisFrame = TRUE;
            hitFlash = 30;
        }
    }

    if (hitThisFrame) drawHits();
    if (hitFlash) hitFlash--;
}

static void updateSprites(void)
{
    u16 idx = 0;

    // Player (blinks after being hit)
    if (!hitFlash || !(frameCounter & 2))
    {
        VDP_setSpriteFull(idx, playerX, playerY, SPRITE_SIZE(2, 2),
                          TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, TILE_PLAYER), idx + 1);
        idx++;
    }

    // Enemy
    VDP_setSpriteFull(idx, enemyX, enemyY, SPRITE_SIZE(2, 2),
                      TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, TILE_ENEMY), idx + 1);
    idx++;

    // Bullets
    const u16 battr = TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, TILE_BULLET);
    for (u16 i = 0; i < MAX_BULLETS; i++)
    {
        Bullet *b = &bullets[i];
        if (!b->active) continue;
        VDP_setSpriteFull(idx, (s16)(b->x >> FP_SHIFT), (s16)(b->y >> FP_SHIFT),
                          SPRITE_SIZE(1, 1), battr, idx + 1);
        idx++;
    }

    // Terminate the link chain
    VDP_setSpriteLink(idx - 1, 0);
    VDP_updateSprites(idx, DMA_QUEUE);
}

static void setupPalette(void)
{
    // PAL0: background + text
    PAL_setColor(0,  RGB24_TO_VDPCOLOR(0x000020));
    PAL_setColor(15, RGB24_TO_VDPCOLOR(0xFFFFFF));

    // PAL1: sprites
    PAL_setColor(16 + 1, RGB24_TO_VDPCOLOR(0x00E0E0)); // player body
    PAL_setColor(16 + 2, RGB24_TO_VDPCOLOR(0xFFFFFF)); // cockpit
    PAL_setColor(16 + 3, RGB24_TO_VDPCOLOR(0x2060FF)); // player core
    PAL_setColor(16 + 4, RGB24_TO_VDPCOLOR(0xE00000)); // bullet rim
    PAL_setColor(16 + 5, RGB24_TO_VDPCOLOR(0xFF8000)); // bullet mid
    PAL_setColor(16 + 6, RGB24_TO_VDPCOLOR(0xFFFF60)); // bullet core
    PAL_setColor(16 + 7, RGB24_TO_VDPCOLOR(0xA020C0)); // enemy body
    PAL_setColor(16 + 8, RGB24_TO_VDPCOLOR(0xFFE000)); // enemy eyes
}

int main(bool hardReset)
{
    (void)hardReset;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    setupPalette();

    VDP_loadTileData(playerTiles, TILE_PLAYER, 4, CPU);
    VDP_loadTileData(enemyTiles,  TILE_ENEMY,  4, CPU);
    VDP_loadTileData(bulletTile,  TILE_BULLET, 1, CPU);

    for (u16 i = 0; i < MAX_BULLETS; i++) bullets[i].active = FALSE;
    bulletCount = 0;

    playerX = (SCREEN_W / 2) - 8;
    playerY = SCREEN_H - 40;
    enemyX = (SCREEN_W / 2) - 8;
    enemyY = 16;
    enemyDir = 1;
    hits = 0;
    hitFlash = 0;
    fireTimer = 0;
    frameCounter = 0;

    VDP_drawText("HITS:", 1, 1);
    drawHits();
    VDP_drawText("DODGE!", 33, 1);

    while (TRUE)
    {
        updatePlayer();
        updateEnemy();
        updateBullets();
        updateSprites();

        frameCounter++;
        SYS_doVBlankProcess();
    }

    return 0;
}
