#include <genesis.h>

#define NUM_SEGMENTS   30
#define SEG_DELAY      8        /* frames between one segment and the next */
#define SPEED          2        /* pixels per frame (8 frames * 2 px = 16 px spacing) */
#define HIST_SIZE      256      /* must be a power of two, >= 29*8+1 */

#define SCREEN_W       320
#define SCREEN_H       224
#define SEG_SIZE       16
#define MIN_X          (-32)    /* keep sprite X + 128 positive (9-bit VDP field) */

/* 16x16 segment = 2x2 tiles, stored column-major: TL, BL, TR, BR.
   Color 1 = outline, color 2 = fill. */
static const u32 segTiles[4 * 8] =
{
    /* top-left */
    0x00011111,
    0x00112222,
    0x01122222,
    0x01222222,
    0x11222222,
    0x12222222,
    0x12222222,
    0x12222222,

    /* bottom-left */
    0x12222222,
    0x12222222,
    0x12222222,
    0x11222222,
    0x01222222,
    0x01122222,
    0x00112222,
    0x00011111,

    /* top-right */
    0x11111000,
    0x22221100,
    0x22222110,
    0x22222210,
    0x22222211,
    0x22222221,
    0x22222221,
    0x22222221,

    /* bottom-right */
    0x22222221,
    0x22222221,
    0x22222221,
    0x22222211,
    0x22222210,
    0x22222110,
    0x22221100,
    0x11111000
};

/* Ring buffer of the head's position, one entry per frame */
static s16 histX[HIST_SIZE];
static s16 histY[HIST_SIZE];
static u8  histHead;

static s16 clampS16(s16 v, s16 lo, s16 hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void updateSprites(void)
{
    s16 px[NUM_SEGMENTS];
    s16 py[NUM_SEGMENTS];
    u16 i, j;
    u16 n = 0;

    for (i = 0; i < NUM_SEGMENTS; i++)
    {
        u8 idx = (u8)(histHead - (i * SEG_DELAY));
        s16 x = histX[idx];
        s16 y = histY[idx];
        u16 pal = (i == 0) ? PAL1 : PAL0;   /* head gets its own palette */
        bool skip = FALSE;

        /* fully off-screen: invisible, but would still eat the dot budget */
        if (x <= -SEG_SIZE || x >= SCREEN_W || y <= -SEG_SIZE || y >= SCREEN_H)
            skip = TRUE;

        /* exactly under an earlier (on top) segment: completely hidden */
        for (j = 0; !skip && j < n; j++)
            if (px[j] == x && py[j] == y) skip = TRUE;

        if (skip) continue;

        px[n] = x;
        py[n] = y;

        VDP_setSpriteFull(n, x, y,
                          SPRITE_SIZE(2, 2),
                          TILE_ATTR_FULL(pal, TRUE, FALSE, FALSE, TILE_USER_INDEX),
                          (u8)(n + 1));
        n++;
    }

    /* terminate the list */
    VDP_setSpriteLink(n - 1, 0);
    VDP_updateSprites(n, DMA_QUEUE);
}

int main(bool hardReset)
{
    s16 headX, headY;
    s16 dirX = 1, dirY = 0;     /* starts moving right */
    u16 k;

    (void)hardReset;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    /* Palettes: PAL0 = green body, PAL1 = red head */
    PAL_setColor(0,  0x0000);
    PAL_setColor(1,  0x0060);   /* dark green */
    PAL_setColor(2,  0x00C0);   /* green */
    PAL_setColor(17, 0x0006);   /* dark red */
    PAL_setColor(18, 0x022E);   /* red */

    VDP_loadTileData(segTiles, TILE_USER_INDEX, 4, CPU);

    VDP_resetSprites();

    /* Start in the middle row, as a straight snake trailing to the left */
    headX = (SCREEN_W / 2) - (SEG_SIZE / 2);
    headY = (SCREEN_H / 2) - (SEG_SIZE / 2);

    for (k = 0; k < HIST_SIZE; k++)
    {
        /* entry HIST_SIZE-1 is the newest (head), older ones go further left */
        s16 age = (s16)(HIST_SIZE - 1 - k);
        s16 x = headX - age * SPEED;
        histX[k] = (x < MIN_X) ? MIN_X : x;
        histY[k] = headY;
    }
    histHead = HIST_SIZE - 1;

    updateSprites();

    while (TRUE)
    {
        u16 pad = JOY_readJoypad(JOY_1);
        s16 dx = 0, dy = 0;

        if (pad & BUTTON_RIGHT) dx += 1;
        if (pad & BUTTON_LEFT)  dx -= 1;
        if (pad & BUTTON_DOWN)  dy += 1;
        if (pad & BUTTON_UP)    dy -= 1;

        /* keep heading until a new direction is pressed */
        if (dx != 0 || dy != 0)
        {
            dirX = dx;
            dirY = dy;
        }

        headX = clampS16(headX + dirX * SPEED, 0, SCREEN_W - SEG_SIZE);
        headY = clampS16(headY + dirY * SPEED, 0, SCREEN_H - SEG_SIZE);

        histHead++;                 /* u8 wraps at 256 */
        histX[histHead] = headX;
        histY[histHead] = headY;

        updateSprites();
        SYS_doVBlankProcess();
    }

    return 0;
}
