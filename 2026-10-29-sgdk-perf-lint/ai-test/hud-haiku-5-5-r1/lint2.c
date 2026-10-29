#include <genesis.h>

/* ---------- Layout / tuning ---------- */
#define HUD_TILE_BASE    TILE_USER_INDEX  /* first VRAM tile used by the HUD bar */
#define HUD_TILE_COUNT   9                /* tile 0 = empty, tiles 1..8 = 1..8 px filled */
#define BAR_SEGMENTS     10               /* number of 8x8 tiles in the health bar */
#define BAR_MAX          (BAR_SEGMENTS * 8)   /* health in pixels: 80 */
#define START_LIVES      3
#define START_TIME_SEC   180              /* 3:00 countdown */
#define SCORE_MAX        999999UL

/* ---------- HUD tile graphics (4bpp, 8x8, 8 rows of u32 each) ----------
 * Each tile is filled from the left. Colour index 1 = green (PAL0),
 * colour index 0 = transparent/background.
 * Row value: pixel i (left to right) occupies nibble (28 - 4*i). */
static const u32 hud_tiles[HUD_TILE_COUNT * 8] =
{
    /* tile 0: empty */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    /* tile 1: 1 px filled */
    0x10000000, 0x10000000, 0x10000000, 0x10000000,
    0x10000000, 0x10000000, 0x10000000, 0x10000000,
    /* tile 2: 2 px filled */
    0x11000000, 0x11000000, 0x11000000, 0x11000000,
    0x11000000, 0x11000000, 0x11000000, 0x11000000,
    /* tile 3: 3 px filled */
    0x11100000, 0x11100000, 0x11100000, 0x11100000,
    0x11100000, 0x11100000, 0x11100000, 0x11100000,
    /* tile 4: 4 px filled */
    0x11110000, 0x11110000, 0x11110000, 0x11110000,
    0x11110000, 0x11110000, 0x11110000, 0x11110000,
    /* tile 5: 5 px filled */
    0x11111000, 0x11111000, 0x11111000, 0x11111000,
    0x11111000, 0x11111000, 0x11111000, 0x11111000,
    /* tile 6: 6 px filled */
    0x11111100, 0x11111100, 0x11111100, 0x11111100,
    0x11111100, 0x11111100, 0x11111100, 0x11111100,
    /* tile 7: 7 px filled */
    0x11111110, 0x11111110, 0x11111110, 0x11111110,
    0x11111110, 0x11111110, 0x11111110, 0x11111110,
    /* tile 8: full */
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111
};

/* Powers of ten that fit in a u32 (10^0 .. 10^9). */
static const u32 pow10Table[10] =
{
    1UL, 10UL, 100UL, 1000UL, 10000UL,
    100000UL, 1000000UL, 10000000UL, 100000000UL, 1000000000UL
};

/* ---------- Helpers ---------- */

/* Draw an unsigned value as a fixed-width, zero-padded decimal number.
 * Only the low `digits` decimal digits are shown. */
static void drawNumber(u16 x, u16 y, u32 value, u16 digits)
{
    char buf[12];
    u16 i;

    /* Reduce to the low `digits` digits. Never taken for this game's
     * values (score is clamped to 999999, lives <= 3), so the slow
     * 32-bit modulo stays off the hot path. */
    if (digits < 10 && value >= pow10Table[digits])
    {
        value %= pow10Table[digits];
    }

    /* Extract digits most-significant first using only subtract/compare.
     * Each digit is at most 9 subtractions, which is far cheaper on the
     * 68000 than a 32-bit divide per digit. */
    for (i = 0; i < digits; i++)
    {
        u32 p = pow10Table[digits - 1 - i];
        u16 d = 0;

        while (value >= p)
        {
            value -= p;
            d++;
        }
        buf[i] = '0' + (char)d;
    }
    buf[digits] = '\0';

    VDP_drawTextBG(BG_A, buf, x, y);
}

/* Draw the countdown as MM:SS. */
static void drawTimer(u16 seconds)
{
    char buf[6];
    u16 minutes = seconds / 60;
    u16 secs    = seconds % 60;

    buf[0] = '0' + (char)((minutes / 10) % 10);
    buf[1] = '0' + (char)(minutes % 10);
    buf[2] = ':';
    buf[3] = '0' + (char)(secs / 10);
    buf[4] = '0' + (char)(secs % 10);
    buf[5] = '\0';

    VDP_drawTextBG(BG_A, buf, 7, 2);
}

/* Draw the health bar: each segment shows 0..8 filled pixels.
 * Segments are placed on plane A starting at column 10, row 8. */
static void drawHealth(u16 health)
{
    u16 i;

    for (i = 0; i < BAR_SEGMENTS; i++)
    {
        s16 remaining = (s16)health - (s16)(i * 8);
        u16 fill;

        if (remaining <= 0)      fill = 0;
        else if (remaining >= 8) fill = 8;
        else                     fill = (u16)remaining;

        VDP_setTileMapXY(BG_A,
                         TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, HUD_TILE_BASE + fill),
                         10 + i, 8);
    }
}

/* ---------- Entry point ---------- */
int main(bool hardReset)
{
    /* Palette 0: 0 = background (black), 1 = green (bar), 2 = red, 3 = yellow,
     * 15 = white (text font colour). Genesis colour format: 0000BBB0GGG0RRR0. */
    u16 palette[16] =
    {
        0x0000, 0x00E0, 0x000E, 0x00EE,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0EEE
    };

    u16 joy;
    u16 fpsTarget;
    u16 frameTick = 0;
    u16 timeLeft  = START_TIME_SEC;
    u32 score     = 0;
    u16 lives     = START_LIVES;
    u16 health    = BAR_MAX;

    /* Last values written to the screen. Sentinels force the first draw. */
    u16 shownTime   = 0xFFFF;
    u32 shownScore  = 0xFFFFFFFFUL;
    u16 shownLives  = 0xFFFF;
    u16 shownHealth = 0xFFFF;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    PAL_setPalette(PAL0, palette, DMA);
    VDP_loadTileData(hud_tiles, HUD_TILE_BASE, HUD_TILE_COUNT, FALSE);

    /* Static labels */
    VDP_drawTextBG(BG_A, "TIME",   2, 2);
    VDP_drawTextBG(BG_A, "SCORE",  2, 4);
    VDP_drawTextBG(BG_A, "LIVES",  2, 6);
    VDP_drawTextBG(BG_A, "HEALTH", 2, 8);

    fpsTarget = IS_PAL_SYSTEM ? 50 : 60;

    while (TRUE)
    {
        joy = JOY_readJoypad(JOY_1);

        /* A held: +10 score every frame */
        if (joy & BUTTON_A)
        {
            score += 10;
            if (score > SCORE_MAX) score = SCORE_MAX;
        }

        /* B held: health bar shrinks by 1 px per frame */
        if ((joy & BUTTON_B) && health > 0)
        {
            health--;
        }

        /* Bar emptied: lose a life and refill (only if lives remain) */
        if (health == 0 && lives > 0)
        {
            lives--;
            health = BAR_MAX;
        }

        /* Countdown: one second per fpsTarget frames, stops at 00:00 */
        frameTick++;
        if (frameTick >= fpsTarget)
        {
            frameTick = 0;
            if (timeLeft > 0) timeLeft--;
        }

        /* Draw HUD: only touch the plane when a displayed value changed */
        if (timeLeft != shownTime)
        {
            drawTimer(timeLeft);
            shownTime = timeLeft;
        }
        if (score != shownScore)
        {
            drawNumber(8, 4, score, 6);
            shownScore = score;
        }
        if (lives != shownLives)
        {
            drawNumber(8, 6, lives, 2);
            shownLives = lives;
        }
        if (health != shownHealth)
        {
            drawHealth(health);
            shownHealth = health;
        }

        SYS_doVBlankProcess();
    }

    return 0;
}
