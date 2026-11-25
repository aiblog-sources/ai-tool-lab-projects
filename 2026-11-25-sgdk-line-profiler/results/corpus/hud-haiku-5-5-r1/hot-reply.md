```c
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

/* Powers of ten, used for division-free digit extraction. */
static const u32 pow10Tab[7] =
{
    1UL, 10UL, 100UL, 1000UL, 10000UL, 100000UL, 1000000UL
};

/* ---------- Cached HUD state ----------
 * Each value holds what is currently shown on screen. A sentinel that can
 * never occur in normal play forces the first draw. Text and tiles are only
 * rewritten when their value changes; unchanged VRAM stays identical. */
static u32 shownScore   = 0xFFFFFFFFUL;
static u16 shownLives   = 0xFFFF;
static u16 shownTime    = 0xFFFF;
static u16 shownHealth  = 0xFFFF;
static u16 shownFill[BAR_SEGMENTS] =
{
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

/* ---------- Helpers ---------- */

/* Draw an unsigned value as a fixed-width, zero-padded decimal number.
 * Caller guarantees value < 10^digits (true for all values used here). */
static void drawNumber(u16 x, u16 y, u32 value, u16 digits)
{
    char buf[12];
    u16 i;

    for (i = 0; i < digits; i++)
    {
        u32 p = pow10Tab[digits - 1 - i];
        u16 c = 0;

        /* Count how many times this place value fits; no division needed. */
        while (value >= p)
        {
            value -= p;
            c++;
        }
        buf[i] = '0' + (char)c;
    }
    buf[digits] = '\0';

    VDP_drawTextBG(BG_A, buf, x, y);
}

/* Draw the countdown as MM:SS. */
static void drawTimer(u16 seconds)
{
    char buf[6];
    u16 minutes;
    u16 secs;

    if (seconds == shownTime) return;
    shownTime = seconds;

    minutes = seconds / 60;
    secs    = seconds % 60;

    buf[0] = '0' + (char)((minutes / 10) % 10);
    buf[1] = '0' + (char)(minutes % 10);
    buf[2] = ':';
    buf[3] = '0' + (char)(secs / 10);
    buf[4] = '0' + (char)(secs % 10);
    buf[5] = '\0';

    VDP_drawTextBG(BG_A, buf, 7, 2);
}

/* Draw the health bar: each segment shows 0..8 filled pixels.
 * Only segments whose fill changed are rewritten. */
static void drawHealth(u16 health)
{
    u16 i;

    if (health == shownHealth) return;
    shownHealth = health;

    for (i = 0; i < BAR_SEGMENTS; i++)
    {
        s16 remaining = (s16)health - (s16)(i * 8);
        u16 fill;

        if (remaining <= 0)      fill = 0;
        else if (remaining >= 8) fill = 8;
        else                     fill = (u16)remaining;

        if (fill != shownFill[i])
        {
            shownFill[i] = fill;
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
    u32 score     = 0;
    u16 lives     = START_LIVES;
    u16 health    = BAR_MAX;

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

        /* Draw HUD (each item redraws only when its value has changed) */
        drawTimer(timeLeft);

        if (score != shownScore)
        {
            shownScore = score;
            drawNumber(8, 4, score, 6);
        }

        if (lives != shownLives)
        {
            shownLives = lives;
            drawNumber(8, 6, lives, 2);
        }

        drawHealth(health);

        SYS_doVBlankProcess();
    }

    return 0;
}
```

Changes, all aimed at the measured hot spots and with no change to what appears on screen:

- **Digit extraction (about 10,200 cycles/frame before):** `drawNumber` no longer divides. It counts how many times each power of ten fits, using a small table, so the software `__umodsi3`/`__udivsi3` calls are gone. Every value passed in is below 10^digits (score is capped at 999999, lives is at most 3), so the output is identical.
- **Text redraws (about 5,700 cycles/frame before):** the timer, score and lives are redrawn only when their value changes. Unchanged tilemap content is left alone, so VRAM ends up the same.
- **Health bar (about 2,700 cycles/frame before, plus the per-segment math):** the bar is skipped when health is unchanged, and within a redraw only segments whose fill changed get a `VDP_setTileMapXY` call. Health drops one pixel at a time, so typically one or two tiles are written instead of ten.
- **`SYS_doVBlankProcess` is unchanged.** It is the frame sync, so its cost is the wait for vblank, not wasted work.

I have not compiled or run this against SGDK 2.09 or the profiler, so please check it in your emulator and confirm the cycle counts.