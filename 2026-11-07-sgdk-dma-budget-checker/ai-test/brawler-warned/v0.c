/*
 * Beat-em-up scene for Sega Mega Drive / Genesis, SGDK 2.11, single file, no resources.
 *
 * - 5 characters, each 48x64 px (6x8 tiles), each with its own 8-frame walk cycle.
 *   Every character steps its frame every 6 video frames. Their timers are phase-offset
 *   (0..4), so no two characters ever change frame on the same video frame.
 * - The player (character 0, red shirt) moves with the d-pad on pad 1. The other four patrol.
 * - All graphics are generated in code at startup: the walk frames are drawn procedurally
 *   into RAM and the background tiles are built at startup.
 *
 * Frame art is stored per design, not per character, because 5 x 8 x 1536 bytes would not
 * fit in 64 KB RAM. There are 3 designs (slim / bulky / long-haired) x 8 frames = 36 KB.
 * The 5 characters are different design + palette combinations, and each has its own
 * animation state, position, speed and direction.
 *
 * Streaming: each character owns a fixed 48-tile slot in VRAM. When its frame changes,
 * the 1536-byte frame is sent by queued DMA (at most 1536 bytes per video frame, plus the
 * 640-byte sprite table), well inside the NTSC VBlank budget. DMA is only executed in VBlank.
 *
 * Sprites: each character is 4 hardware sprites (32x32, 16x32, 32x32, 16x32), 20 in total.
 * That is at most 10 sprites, 240 px and 30 cells on any scanline, so the limits
 * (20 / 320 / 40) are never exceeded and nothing flickers.
 */

#include <genesis.h>

#define NCHAR        5
#define NDESIGN      3
#define NFRAMES      8
#define FW           48
#define FH           64
#define CHAR_TILES   48                 /* 6 x 8 tiles */
#define ANIM_PERIOD  6                  /* video frames per walk frame */
#define IDLE_FRAME   2

#define BG_TILE_COUNT 17
#define BG_BASE      (TILE_USER_INDEX)
#define CHAR_BASE    (BG_BASE + 20)

#define T_BRICK 0
#define T_BASE  1
#define T_FLOOR 2
#define T_CURB  3
#define T_ROOF  4
#define T_WIN   5                       /* 12 tiles: 3 wide x 4 tall */

#define MDC(r,g,b) ((u16)(((((b) >> 5) & 7) << 9) | ((((g) >> 5) & 7) << 5) | ((((r) >> 5) & 7) << 1)))

/* ------------------------------------------------------------------------- */
/* Palettes                                                                  */
/* ------------------------------------------------------------------------- */

static const u16 palBg[16] = {
    MDC(0x30,0x60,0xC0),    /* 0 sky / backdrop */
    MDC(0xA0,0x48,0x30),    /* 1 brick */
    MDC(0x58,0x28,0x20),    /* 2 mortar */
    MDC(0x60,0x60,0x68),    /* 3 baseboard */
    MDC(0x90,0x90,0x98),    /* 4 floor */
    MDC(0x68,0x68,0x70),    /* 5 seams */
    MDC(0xD8,0xD0,0xB0),    /* 6 window frame */
    MDC(0x20,0x40,0xA0),    /* 7 glass */
    MDC(0x80,0xB0,0xE0),    /* 8 glass highlight */
    MDC(0xC8,0xC8,0xD0),    /* 9 light edge */
    MDC(0x40,0x40,0x48),    /* 10 roof */
    MDC(0xB8,0x5A,0x40),    /* 11 brick highlight */
    0, 0, 0, 0
};

/* 1 outline, 2 skin, 3 hair, 4 shirt, 5 trim, 6 pants, 7 shoes, 8 white */
static const u16 palChar[3][16] = {
    { 0, MDC(0x20,0x10,0x20), MDC(0xE8,0xB8,0x90), MDC(0x40,0x20,0x10), MDC(0xD0,0x20,0x20),
         MDC(0x80,0x10,0x10), MDC(0x20,0x40,0xA0), MDC(0x30,0x20,0x20), MDC(0xF0,0xF0,0xF0),
         0,0,0,0,0,0,0 },
    { 0, MDC(0x20,0x10,0x20), MDC(0xA0,0x68,0x40), MDC(0x18,0x18,0x18), MDC(0x30,0xA0,0x40),
         MDC(0x18,0x60,0x28), MDC(0x50,0x48,0x38), MDC(0xE0,0xE0,0xE0), MDC(0xF0,0xF0,0xF0),
         0,0,0,0,0,0,0 },
    { 0, MDC(0x20,0x10,0x20), MDC(0xF0,0xC8,0xA0), MDC(0xE0,0xC0,0x30), MDC(0x80,0x30,0xB0),
         MDC(0x50,0x18,0x70), MDC(0x20,0x20,0x20), MDC(0xA0,0x30,0x20), MDC(0xF0,0xF0,0xF0),
         0,0,0,0,0,0,0 }
};

/* ------------------------------------------------------------------------- */
/* Procedural character art                                                  */
/* ------------------------------------------------------------------------- */

typedef struct {
    u8 tw;      /* torso width  */
    u8 lw;      /* leg width    */
    u8 aw;      /* arm width    */
    u8 hw;      /* head width   */
    u8 hair;    /* 0 bald+beard+shades, 1 spiky, 2 long + headband */
} Design;

static const Design designs[NDESIGN] = {
    { 12, 7, 5, 14, 1 },
    { 18, 9, 7, 16, 0 },
    { 14, 7, 5, 14, 2 }
};

static const s8 swingT[8] = { 0, 4, 7, 4, 0, -4, -7, -4 };
static const u8 liftT[8]  = { 3, 2, 0, 0, 0, 0, 0, 2 };
static const u8 bobT[8]   = { 0, 1, 2, 1, 0, 1, 2, 1 };

static u8  pix[FH][FW];                                     /* scratch canvas, 1 byte / pixel */
static u32 frames[NDESIGN][NFRAMES][CHAR_TILES * 8];        /* 36 KB of ready-to-DMA tile data */

static void rect(s16 x, s16 y, s16 w, s16 h, u8 c)
{
    s16 x1 = x + w, y1 = y + h, xx, yy;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > FW) x1 = FW;
    if (y1 > FH) y1 = FH;
    for (yy = y; yy < y1; yy++)
        for (xx = x; xx < x1; xx++)
            pix[yy][xx] = c;
}

static void orect(s16 x, s16 y, s16 w, s16 h, u8 c)
{
    rect(x - 1, y - 1, w + 2, h + 2, 1);
    rect(x, y, w, h, c);
}

/* Limb hanging from (sx,y0), whose end is displaced by sw pixels; tip = hand / shoe. */
static void limb(s16 sx, s16 y0, s16 len, s16 sw, s16 w, u8 col, s16 tipRows, u8 tipCol, s16 tipExtra)
{
    s16 i, x = sx - w / 2, ww = w;
    for (i = 0; i < len; i++)
    {
        u8 c = col;
        x = sx + (sw * i) / len - w / 2;
        ww = w;
        if (i >= len - tipRows) { c = tipCol; ww = w + tipExtra; }
        rect(x - 1, y0 + i, ww + 2, 1, 1);
        rect(x, y0 + i, ww, 1, c);
    }
    rect(x - 1, y0 + len, ww + 2, 1, 1);
}

static void packRegion(u32 **out, s16 tx0, s16 ty0, s16 wt, s16 ht)
{
    s16 c, r, y, x;
    for (c = 0; c < wt; c++)                 /* hardware sprites are column-major */
        for (r = 0; r < ht; r++)
            for (y = 0; y < 8; y++)
            {
                const u8 *p = &pix[(ty0 + r) * 8 + y][(tx0 + c) * 8];
                u32 v = 0;
                for (x = 0; x < 8; x++) v = (v << 4) | (p[x] & 15);
                *(*out)++ = v;
            }
}

static void genFrame(const Design *d, u8 f, u32 *out)
{
    s16 cx = 24;
    s16 bob = bobT[f];
    u8 kA = f & 7;
    u8 kB = (f + 4) & 7;
    s16 hipY = 38 + bob;
    s16 hx = cx - d->hw / 2 + 1;
    s16 hw = d->hw;
    s16 tx = cx - d->tw / 2;

    memset(pix, 0, sizeof(pix));

    /* back leg, back arm */
    limb(cx, hipY, 62 - hipY - liftT[kB], (swingT[kB] * 3) / 2, d->lw, 6, 4, 7, 3);
    limb(cx, 24 + bob, 15, swingT[kA], d->aw, 5, 6, 2, 0);

    /* front leg */
    limb(cx, hipY, 62 - hipY - liftT[kA], (swingT[kA] * 3) / 2, d->lw, 6, 4, 7, 3);

    /* long hair hangs behind head and torso */
    if (d->hair == 2) orect(hx - 3, 5 + bob, 7, 24, 3);

    /* torso + belt + neck */
    orect(cx - 2, 18 + bob, 5, 5, 2);
    orect(tx, 22 + bob, d->tw, 18, 4);
    rect(tx, 36 + bob, d->tw, 3, 5);

    /* spiky hair spikes sit behind the cap */
    if (d->hair == 1)
    {
        orect(hx, bob, 4, 3, 3);
        orect(hx + 6, bob, 4, 3, 3);
    }

    /* head, nose */
    orect(hx, 5 + bob, hw, 15, 2);
    rect(hx + hw + 1, 11 + bob, 1, 3, 1);
    rect(hx + hw, 11 + bob, 1, 3, 2);

    /* face */
    if (d->hair == 0)
    {
        rect(hx + hw - 9, 9 + bob, 9, 3, 1);            /* sunglasses */
        rect(hx + hw - 9, 16 + bob, 10, 4, 3);          /* beard */
    }
    else
    {
        rect(hx + hw - 6, 9 + bob, 3, 3, 8);
        rect(hx + hw - 4, 10 + bob, 1, 2, 1);
        rect(hx + hw - 5, 16 + bob, 3, 1, 1);
    }

    /* hair cap */
    if (d->hair == 1)
    {
        orect(hx - 1, 3 + bob, hw + 2, 6, 3);
        rect(hx, 9 + bob, 3, 4, 3);
    }
    else if (d->hair == 2)
    {
        rect(hx, 5 + bob, hw, 3, 3);
        rect(hx + hw - 4, 5 + bob, 4, 4, 3);
        rect(hx - 5, 8 + bob + (f & 1), 5, 2, 5);       /* headband tail */
        rect(hx, 8 + bob, hw, 2, 5);                    /* headband */
    }

    /* front arm */
    limb(cx, 24 + bob, 15, swingT[kB], d->aw, 5, 6, 2, 0);

    /* pack into the sprite tile order: 32x32 | 16x32 | 32x32 | 16x32 */
    {
        u32 *p = out;
        packRegion(&p, 0, 0, 4, 4);
        packRegion(&p, 4, 0, 2, 4);
        packRegion(&p, 0, 4, 4, 4);
        packRegion(&p, 4, 4, 2, 4);
    }
}

/* ------------------------------------------------------------------------- */
/* Procedural background tiles                                               */
/* ------------------------------------------------------------------------- */

static u8  bt[8][8];
static u32 bgTiles[BG_TILE_COUNT * 8];

static void packBg(u16 n)
{
    s16 y, x;
    for (y = 0; y < 8; y++)
    {
        u32 v = 0;
        for (x = 0; x < 8; x++) v = (v << 4) | (bt[y][x] & 15);
        bgTiles[n * 8 + y] = v;
    }
}

static void makeBgTiles(void)
{
    s16 x, y, tx, ty;

    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++)
        bt[y][x] = ((y & 3) == 3 || x == (y < 4 ? 3 : 7)) ? 2 : ((y & 3) == 0 ? 11 : 1);
    packBg(T_BRICK);

    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++)
        bt[y][x] = (y < 1) ? 9 : (y < 7 ? 3 : 5);
    packBg(T_BASE);

    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++)
        bt[y][x] = (y == 7 || x == 7) ? 5 : 4;
    packBg(T_FLOOR);

    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++)
        bt[y][x] = (y < 4) ? 9 : (y < 7 ? 4 : 5);
    packBg(T_CURB);

    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++)
        bt[y][x] = (y < 2) ? 9 : (y < 6 ? 10 : 2);
    packBg(T_ROOF);

    for (ty = 0; ty < 4; ty++) for (tx = 0; tx < 3; tx++)
    {
        for (y = 0; y < 8; y++) for (x = 0; x < 8; x++)
        {
            s16 X = tx * 8 + x, Y = ty * 8 + y;
            u8 c;
            if (X < 2 || X >= 22 || Y < 2 || Y >= 30 || (X >= 11 && X <= 12) || (Y >= 15 && Y <= 16))
                c = 6;
            else
                c = (((X + Y) % 10) < 2) ? 8 : 7;
            bt[y][x] = c;
        }
        packBg(T_WIN + ty * 3 + tx);
    }
}

static void drawScene(void)
{
    s16 i, tx, ty;
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    VDP_fillTileMapRect(BG_B, TILE_ATTR_FULL(PAL0, 0, 0, 0, BG_BASE + T_ROOF),  0, 4, 40, 1);
    VDP_fillTileMapRect(BG_B, TILE_ATTR_FULL(PAL0, 0, 0, 0, BG_BASE + T_BRICK), 0, 5, 40, 12);
    VDP_fillTileMapRect(BG_B, TILE_ATTR_FULL(PAL0, 0, 0, 0, BG_BASE + T_BASE),  0, 17, 40, 1);
    VDP_fillTileMapRect(BG_B, TILE_ATTR_FULL(PAL0, 0, 0, 0, BG_BASE + T_CURB),  0, 18, 40, 1);
    VDP_fillTileMapRect(BG_B, TILE_ATTR_FULL(PAL0, 0, 0, 0, BG_BASE + T_FLOOR), 0, 19, 40, 9);

    for (i = 0; i < 4; i++)
        for (ty = 0; ty < 4; ty++)
            for (tx = 0; tx < 3; tx++)
                VDP_setTileMapXY(BG_B,
                    TILE_ATTR_FULL(PAL0, 0, 0, 0, BG_BASE + T_WIN + ty * 3 + tx),
                    3 + i * 10 + tx, 7 + ty);
}

/* ------------------------------------------------------------------------- */
/* Characters                                                                */
/* ------------------------------------------------------------------------- */

typedef struct {
    s16  x, y;
    s16  minx, maxx;
    s8   dir;
    u8   design;
    u8   pal;
    u8   frame;
    u8   timer;
    u8   periodMask;    /* NPC moves when (tick & mask) == 0 */
    u8   flip;
} Char;

static Char ch[NCHAR];

#define X_MIN 0
#define X_MAX (320 - 48)
#define Y_MIN 96
#define Y_MAX 152

static void initChar(u8 i, u8 design, u8 pal, s16 x, s16 y, s16 minx, s16 maxx, s8 dir, u8 mask)
{
    Char *c = &ch[i];
    c->x = x; c->y = y;
    c->minx = minx; c->maxx = maxx;
    c->dir = dir;
    c->design = design;
    c->pal = pal;
    c->frame = (i == 0) ? IDLE_FRAME : (u8)((i * 3) & 7);
    c->timer = i + 1;               /* phases 0..4 of the 6-frame period: never in step */
    c->periodMask = mask;
    c->flip = (dir < 0);
}

static void queueFrame(u8 i)
{
    Char *c = &ch[i];
    DMA_queueDma(DMA_VRAM, (void *)frames[c->design][c->frame],
                 (u16)((CHAR_BASE + i * CHAR_TILES) * 32), CHAR_TILES * 16, 2);
}

int main(bool hardReset)
{
    u16 tick = 0;
    u8 i, d, f;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    /* palettes */
    PAL_setPalette(PAL0, palBg, CPU);
    PAL_setPalette(PAL1, palChar[0], CPU);
    PAL_setPalette(PAL2, palChar[1], CPU);
    PAL_setPalette(PAL3, palChar[2], CPU);

    /* background */
    makeBgTiles();
    VDP_loadTileData(bgTiles, BG_BASE, BG_TILE_COUNT, CPU);
    drawScene();

    /* character art */
    for (d = 0; d < NDESIGN; d++)
        for (f = 0; f < NFRAMES; f++)
            genFrame(&designs[d], f, frames[d][f]);

    /* player + 4 patrolling NPCs (design, palette index 1..3) */
    initChar(0, 0, 1, 136, 132,   X_MIN, X_MAX,  1, 0);
    initChar(1, 1, 2,  20, 108,    8, 140,  1, 0);
    initChar(2, 2, 3, 200, 124,  100, 272, -1, 1);
    initChar(3, 0, 2,  60, 140,    0, 200,  1, 0);
    initChar(4, 1, 3, 240, 152,  120, 272, -1, 1);

    for (i = 0; i < NCHAR; i++)
        VDP_loadTileData(frames[ch[i].design][ch[i].frame], CHAR_BASE + i * CHAR_TILES, CHAR_TILES, CPU);

    VDP_resetSprites();

    for (;;)
    {
        u8 order[NCHAR] = { 0, 1, 2, 3, 4 };
        u16 joy = JOY_readJoypad(JOY_1);
        u8 moving = 0;
        u8 r;
        Char *p = &ch[0];

        tick++;

        /* ---- player ---- */
        if (joy & BUTTON_LEFT)       { p->x -= 2; p->flip = 1; moving = 1; }
        else if (joy & BUTTON_RIGHT) { p->x += 2; p->flip = 0; moving = 1; }
        if (joy & BUTTON_UP)         { p->y -= 1; moving = 1; }
        else if (joy & BUTTON_DOWN)  { p->y += 1; moving = 1; }
        if (p->x < X_MIN) p->x = X_MIN;
        if (p->x > X_MAX) p->x = X_MAX;
        if (p->y < Y_MIN) p->y = Y_MIN;
        if (p->y > Y_MAX) p->y = Y_MAX;

        /* ---- NPC patrol ---- */
        for (i = 1; i < NCHAR; i++)
        {
            Char *c = &ch[i];
            if ((tick & c->periodMask) == 0)
            {
                c->x += c->dir;
                if (c->x <= c->minx) { c->x = c->minx; c->dir = 1; }
                if (c->x >= c->maxx) { c->x = c->maxx; c->dir = -1; }
                c->flip = (c->dir < 0);
            }
        }

        /* ---- animation: each char steps every 6 frames, phase-shifted ---- */
        for (i = 0; i < NCHAR; i++)
        {
            Char *c = &ch[i];
            if (--c->timer == 0)
            {
                u8 nf;
                c->timer = ANIM_PERIOD;
                if (i == 0) nf = moving ? ((c->frame + 1) & 7) : IDLE_FRAME;
                else        nf = (c->frame + 1) & 7;
                if (nf != c->frame)
                {
                    c->frame = nf;
                    queueFrame(i);          /* 1536 bytes, at most once per video frame */
                }
            }
        }

        /* ---- depth sort: larger y (closer) gets the lower sprite index (drawn on top) ---- */
        {
            u8 a, b, k;
            for (a = 1; a < NCHAR; a++)
            {
                k = order[a];
                b = a;
                while (b > 0 && ch[order[b - 1]].y < ch[k].y) { order[b] = order[b - 1]; b--; }
                order[b] = k;
            }
        }

        /* ---- build sprite list: 4 hardware sprites per character ---- */
        for (r = 0; r < NCHAR; r++)
        {
            Char *c = &ch[order[r]];
            u16 base = CHAR_BASE + order[r] * CHAR_TILES;
            u16 pal = c->pal;
            u16 fl = c->flip ? 1 : 0;
            u8 s = r * 4;
            s16 xb = fl ? c->x + 16 : c->x;         /* 32-wide part */
            s16 xs = fl ? c->x : c->x + 32;         /* 16-wide part */

            VDP_setSpriteFull(s,     xb, c->y,      SPRITE_SIZE(4, 4), TILE_ATTR_FULL(pal, 0, 0, fl, base),      s + 1);
            VDP_setSpriteFull(s + 1, xs, c->y,      SPRITE_SIZE(2, 4), TILE_ATTR_FULL(pal, 0, 0, fl, base + 16), s + 2);
            VDP_setSpriteFull(s + 2, xb, c->y + 32, SPRITE_SIZE(4, 4), TILE_ATTR_FULL(pal, 0, 0, fl, base + 24), s + 3);
            VDP_setSpriteFull(s + 3, xs, c->y + 32, SPRITE_SIZE(2, 4), TILE_ATTR_FULL(pal, 0, 0, fl, base + 40),
                              (r == NCHAR - 1) ? 0 : (s + 4));
        }
        VDP_updateSprites(NCHAR * 4, DMA_QUEUE);

        /* waits for VBlank, then flushes the DMA queue (tiles + sprite table) */
        SYS_doVBlankProcess();
    }

    return 0;
}
