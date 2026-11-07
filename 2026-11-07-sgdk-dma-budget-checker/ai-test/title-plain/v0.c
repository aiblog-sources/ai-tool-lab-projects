// main.c - Animated title screen for the Sega Mega Drive (SGDK 2.11)
//
// - Logo: 256x160 px = 32x20 tiles = 640 tiles, generated in code at startup.
// - The 640 tiles are uploaded once. Each tile carries a 10-bit "signature"
//   (a few pixels nudged by one palette step, based on the tile number), so
//   the tiles are distinct.
// - The 4 animation frames are palette rotations of the logo's rainbow bands.
//   A new frame is shown every 8 video frames. Four full-logo tile sets would
//   need 2560 tiles, which does not fit in the 2048 tiles of VRAM.
// - "PRESS START" blinks below the logo. After START is pressed it blinks faster.
// - No resource files are used.

#include <genesis.h>

#define LOGO_W_TILES    32
#define LOGO_H_TILES    20
#define LOGO_TILES      (LOGO_W_TILES * LOGO_H_TILES)   // 640
#define LOGO_W_PX       (LOGO_W_TILES * 8)              // 256
#define LOGO_H_PX       (LOGO_H_TILES * 8)              // 160

#define LOGO_X          4                               // (40 - 32) / 2
#define LOGO_Y          2

#define TEXT_STR        "PRESS START"
#define TEXT_LEN        11
#define TEXT_X          ((40 - TEXT_LEN) / 2)
#define TEXT_Y          24

#define ANIM_FRAMES     4
#define ANIM_PERIOD     8                               // video frames per logo frame

#define NUM_BANDS       12                              // palette indices 1..12 cycle
#define PIX_TEXT        13
#define PIX_SHADOW      14
#define PIX_BORDER      15

// ---------------------------------------------------------------------------
// 5x7 font for the letters used in the logo text "NEON DRIVE"
// ---------------------------------------------------------------------------
static const u8 gN[7] = { 0x11, 0x19, 0x15, 0x15, 0x13, 0x11, 0x11 };
static const u8 gE[7] = { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F };
static const u8 gO[7] = { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E };
static const u8 gD[7] = { 0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E };
static const u8 gR[7] = { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 };
static const u8 gI[7] = { 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E };
static const u8 gV[7] = { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04 };

static const char logoText[] = "NEON DRIVE";
#define LOGO_TEXT_LEN   10
#define GLYPH_SCALE     3
#define GLYPH_ADV       (6 * GLYPH_SCALE)                       // 5 px glyph + 1 px gap, scaled
#define TEXT_PX_W       (LOGO_TEXT_LEN * GLYPH_ADV - GLYPH_SCALE)
#define TEXT_PX_H       (7 * GLYPH_SCALE)
#define TEXT_ORG_X      ((LOGO_W_PX - TEXT_PX_W) / 2)
#define TEXT_ORG_Y      ((LOGO_H_PX - TEXT_PX_H) / 2)

static const u8* glyphFor(char c)
{
    switch (c)
    {
        case 'N': return gN;
        case 'E': return gE;
        case 'O': return gO;
        case 'D': return gD;
        case 'R': return gR;
        case 'I': return gI;
        case 'V': return gV;
        default:  return NULL;
    }
}

// Is the (scaled) logo text lit at logo pixel (x, y)?
static bool textOn(int x, int y)
{
    int lx = x - TEXT_ORG_X;
    int ly = y - TEXT_ORG_Y;

    if (lx < 0 || ly < 0 || lx >= TEXT_PX_W || ly >= TEXT_PX_H) return FALSE;

    int ci = lx / GLYPH_ADV;
    int cx = (lx % GLYPH_ADV) / GLYPH_SCALE;
    int cy = ly / GLYPH_SCALE;
    if (cx >= 5) return FALSE;                  // gap between letters

    const u8* g = glyphFor(logoText[ci]);
    if (!g) return FALSE;

    return (g[cy] >> (4 - cx)) & 1;
}

// ---------------------------------------------------------------------------
// Per-tile signature: 10 scattered pixel positions inside the 8x8 tile.
// Bit b of the tile number nudges the pixel at (sigX[b], sigY[b]) by one
// palette step (only on rainbow-band pixels), making every tile distinct.
// ---------------------------------------------------------------------------
static const u8 sigX[10] = { 1, 6, 3, 0, 5, 2, 7, 4, 2, 7 };
static const u8 sigY[10] = { 2, 5, 0, 3, 6, 1, 4, 7, 2, 5 };

// Palette index (0..15) of logo pixel (x, y)
static u8 logoPixel(int x, int y)
{
    // Frame border (3 px thick, so no tile is entirely border)
    if (x < 3 || y < 3 || x >= LOGO_W_PX - 3 || y >= LOGO_H_PX - 3) return PIX_BORDER;

    // Lettering with drop shadow
    if (textOn(x, y))         return PIX_TEXT;
    if (textOn(x - 2, y - 2)) return PIX_SHADOW;

    // Diagonal rainbow bands (these are what the palette rotation animates)
    u8 idx = 1 + (u8)((((x + y) >> 2)) % NUM_BANDS);

    // Tile signature
    u16 tileNum = (u16)((y >> 3) * LOGO_W_TILES + (x >> 3));
    u8 lx = x & 7;
    u8 ly = y & 7;
    for (u8 b = 0; b < 10; b++)
    {
        if (sigX[b] == lx && sigY[b] == ly)
        {
            if ((tileNum >> b) & 1) idx = 1 + (idx % NUM_BANDS);
            break;
        }
    }
    return idx;
}

// Generate one 8x8 4bpp tile (8 x u32, top row first, leftmost pixel in the high nibble)
static void buildTile(u16 tx, u16 ty, u32* out)
{
    for (u16 row = 0; row < 8; row++)
    {
        u32 v = 0;
        for (u16 col = 0; col < 8; col++)
        {
            u32 c = logoPixel(tx * 8 + col, ty * 8 + row);
            v |= c << (28 - 4 * col);
        }
        out[row] = v;
    }
}

// ---------------------------------------------------------------------------
// Palettes
// ---------------------------------------------------------------------------
static u16 hueRamp[NUM_BANDS];
static u16 logoPal[16];
static u16 textPal[16];

static void initColors(void)
{
    static const u8 ramp[NUM_BANDS][3] = {
        {7,0,0}, {7,3,0}, {7,6,0}, {4,7,0},
        {0,7,0}, {0,7,3}, {0,7,7}, {0,3,7},
        {0,0,7}, {3,0,7}, {7,0,7}, {7,0,3}
    };
    for (u8 i = 0; i < NUM_BANDS; i++)
        hueRamp[i] = RGB3_3_3_TO_VDPCOLOR(ramp[i][0], ramp[i][1], ramp[i][2]);

    for (u8 i = 0; i < 16; i++) textPal[i] = RGB3_3_3_TO_VDPCOLOR(7, 7, 7);
    textPal[0] = 0;
}

// Rotate the rainbow by 3 entries per animation frame (4 frames = full loop)
static void setLogoFrame(u8 frame)
{
    logoPal[0] = 0;
    for (u8 i = 0; i < NUM_BANDS; i++)
        logoPal[1 + i] = hueRamp[(i + frame * (NUM_BANDS / ANIM_FRAMES)) % NUM_BANDS];

    logoPal[PIX_TEXT]   = RGB3_3_3_TO_VDPCOLOR(7, 7, 7);
    logoPal[PIX_SHADOW] = RGB3_3_3_TO_VDPCOLOR(0, 0, 1);
    logoPal[PIX_BORDER] = RGB3_3_3_TO_VDPCOLOR(6, 6, 6);
}

// ---------------------------------------------------------------------------
int main(bool hardReset)
{
    (void)hardReset;

    VDP_setScreenWidth320();
    VDP_setBackgroundColor(0);

    initColors();
    setLogoFrame(0);
    PAL_setPalette(PAL0, logoPal, CPU);
    PAL_setPalette(PAL1, textPal, CPU);
    VDP_setTextPalette(PAL1);

    // Generate and upload the 640 logo tiles (one at a time, no big RAM buffer)
    VDP_setEnable(FALSE);
    {
        u32 tile[8];
        for (u16 ty = 0; ty < LOGO_H_TILES; ty++)
        {
            for (u16 tx = 0; tx < LOGO_W_TILES; tx++)
            {
                buildTile(tx, ty, tile);
                VDP_loadTileData(tile, TILE_USER_INDEX + ty * LOGO_W_TILES + tx, 1, CPU);
            }
        }
    }

    // Map the 640 tiles as a 32x20 block, centered horizontally on plane A
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_fillTileMapRectInc(BG_A,
        TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_USER_INDEX),
        LOGO_X, LOGO_Y, LOGO_W_TILES, LOGO_H_TILES);
    VDP_setEnable(TRUE);

    u16  frameCount  = 0;
    u8   animFrame   = 0;
    bool textVisible = FALSE;
    bool started     = FALSE;
    u16  blinkPeriod = 30;          // video frames per on/off phase
    u16  prevPad     = 0;

    while (TRUE)
    {
        // Logo animation: new frame every 8 video frames
        if ((frameCount % ANIM_PERIOD) == 0)
        {
            animFrame = (frameCount / ANIM_PERIOD) % ANIM_FRAMES;
            setLogoFrame(animFrame);
            PAL_setPalette(PAL0, logoPal, DMA);
        }

        // Controls (pad 1): START speeds up the blinking
        u16 pad = JOY_readJoypad(JOY_1);
        if ((pad & BUTTON_START) && !(prevPad & BUTTON_START))
        {
            started = TRUE;
            blinkPeriod = 4;
        }
        prevPad = pad;

        // PRESS START blink
        bool wantVisible = ((frameCount / blinkPeriod) & 1) == 0;
        if (wantVisible != textVisible)
        {
            textVisible = wantVisible;
            if (textVisible) VDP_drawText(TEXT_STR, TEXT_X, TEXT_Y);
            else             VDP_clearTextArea(TEXT_X, TEXT_Y, TEXT_LEN, 1);
        }

        (void)started;
        frameCount++;
        SYS_doVBlankProcess();
    }

    return 0;
}
