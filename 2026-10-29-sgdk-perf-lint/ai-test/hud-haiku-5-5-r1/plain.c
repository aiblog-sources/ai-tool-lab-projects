#include <genesis.h>

/* ---------- Layout / tuning ---------- */
#define HUD_TILE_BASE    TILE_USER_INDEX  /* first VRAM tile used by the HUD bar */
#define HUD_TILE_COUNT   9                /* tile 0 = empty, tiles 1..8 = 1..8 px filled */
#define BAR_SEGMENTS     10               /* number of 8x8 tiles in the health bar */
#define BAR_MAX          (BAR_SEGMENTS * 8)   /* health in pixels: 80 */
#define START_LIVES      3
#define START_TIME_SEC   180              /* 3:00 countdown */
#define SCORE_MAX        999999UL
#define SCORE_DIGITS     6
#define NO_VALUE         0xFF             /* "never drawn" sentinel for cached values */

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

/* ---------- Cached HUD state ----------
 * Each HUD element is only redrawn when its displayed value changes.
 * The plane contents end up identical to redrawing every frame. */
static u8  scoreDigits[SCORE_DIGITS];   /* index 0 = units, 5 = hundred-thousands */
static u8  scoreDrawn[SCORE_DIGITS];    /* digits currently shown on screen */
static u8  barFill[BAR_SEGMENTS];       /* fill currently shown per segment */
static u16 timeDrawn;                   /* seconds currently shown */
static u16 livesDrawn;                  /* lives currently shown */

/* ---------- Helpers ---------- */

/* Draw an unsigned value as a fixed-width, zero-padded decimal number. */
static void drawNumber(u16 x, u16 y, u32 value, u16 digits)
{
    char buf[12];
    u16 i;

    for (i = 0; i < digits; i++)
    {
        buf[digits - 1 - i] = '0' + (char)(value % 10);
        value /= 10;
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

/* Add 10 to the score held as decimal digits, clamping at SCORE_MAX.
 * No division is needed: a carry out of the top digit means the
 * result exceeds 999999, so the score is clamped to 999999. */
static void addScore10(void)
{
    u16 i = 1;   /* tens digit */

    while (1)
    {
        if (i >= SCORE_DIGITS)
        {
            u16 j;
            for (j = 0; j < SCORE_DIGITS; j++) scoreDigits[j] = 9;
            return;
        }
        if (scoreDigits[i] < 9)
        {
            scoreDigits[i]++;
            return;
        }
        scoreDigits[i] = 0;
        i++;
    }
}

/* Draw only the score digits that changed since last draw. */
static void drawScore(void)
{
    u16 i;

    for (i = 0; i < SCORE_DIGITS; i++)
    {
        if (scoreDigits[i] != scoreDrawn[i])
        {
            char buf[2];
            scoreDrawn[i] = scoreDigits[i];
            buf[0] = '0' + (char)scoreDigits[i];
            buf[1] = '\0';
            VDP_drawTextBG(BG_A, buf, 8 + (SCORE_DIGITS - 1 - i), 4);
        }
    }
}

/* Draw the health bar: each segment shows 0..8 filled pixels.
 * Only segments whose fill changed are written to the tilemap. */
static void drawHealth(u16 health)
{
    u16 i;

    for (i = 0; i < BAR_SEGMENTS; i++)
    {
        s16 remaining = (s16)health - (s16)(i * 8);
        u8 fill;

        if (remaining <= 0)      fill = 0;
        else if (remaining >= 8) fill = 8;
        else                     fill = (u8)remaining;

        if (fill != barFill[i])
        {
            barFill[i] = fill;
            VDP_setTileMapXY(BG_A,
                             TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, HUD_TILE_BASE + fill),
                             10 + i, 8);
        }
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
    u16 lives     = START_LIVES;
    u16 health    = BAR_MAX;
    u16 i;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    PAL_setPalette(PAL0, palette, DMA);
    VDP_loadTileData(hud_tiles, HUD_TILE_BASE, HUD_TILE_COUNT, FALSE);

    /* Initial HUD state: score is 0, and nothing has been drawn yet
     * so the first frame draws every element. */
    for (i = 0; i < SCORE_DIGITS; i++)
    {
        scoreDigits[i] = 0;
        scoreDrawn[i]  = NO_VALUE;
    }
    for (i = 0; i < BAR_SEGMENTS; i++)
    {
        barFill[i] = NO_VALUE;
    }
    timeDrawn  = 0xFFFF;
    livesDrawn = 0xFFFF;

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
            addScore10();
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

        /* Draw HUD (only what changed) */
        if (timeLeft != timeDrawn)
        {
            timeDrawn = timeLeft;
            drawTimer(timeLeft);
        }
        drawScore();
        if (lives != livesDrawn)
        {
            livesDrawn = lives;
            drawNumber(8, 6, lives, 2);
        }
        drawHealth(health);

        SYS_doVBlankProcess();
    }

    return 0;
}
