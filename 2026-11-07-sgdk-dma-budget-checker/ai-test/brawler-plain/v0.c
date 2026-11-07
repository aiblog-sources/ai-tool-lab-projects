// main.c : SGDK 2.11 : Beat-em-up street scene
// 5 fighters, each 48x64 px (6x8 tiles) with an 8-frame walk cycle.
// Each fighter has its own animation timer and frame (changes every 6 video
// frames), so they are not in step with each other.
// Fighter 0 (red, bottom of the screen at start) is controlled with the pad 1 D-pad.
//
// No resource files: all art is drawn procedurally into RAM at startup and
// uploaded to VRAM once. Two body designs x 3 palettes give 5 distinct looks.
// Frames are selected by changing the sprite tile index, so there is no
// per-frame tile DMA, only the sprite table is updated.
//
// Each fighter is 4 hardware sprites: (32x32 + 16x32) top, (32x32 + 16x32) bottom.

#include <genesis.h>

#define CW                  48
#define CH                  64
#define FRAMES              8
#define TILES_PER_FRAME     48
#define TILES_PER_VARIANT   (FRAMES * TILES_PER_FRAME)
#define NUM_VARIANTS        2
#define NUM_ACTORS          5
#define ANIM_PERIOD         6

#define SPR_TILE_BASE       (TILE_USER_INDEX)
#define BG_TILE_BASE        (SPR_TILE_BASE + NUM_VARIANTS * TILES_PER_VARIANT)

#define IABS(a)             ((a) < 0 ? -(a) : (a))
#define RGB(c)              RGB24_TO_VDPCOLOR(c)

/* ------------------------------------------------------------------ */
/* Character palette indices                                          */
/* ------------------------------------------------------------------ */
enum
{
    C_T = 0, C_OL = 1, C_SKIN = 2, C_SKIN_D = 3, C_HAIR = 4,
    C_SHIRT = 5, C_SHIRT_D = 6, C_PANTS = 7, C_PANTS_D = 8,
    C_SHOE = 9, C_BELT = 10, C_WHITE = 11, C_SHADOW = 12
};

static const u16 charPal[3][16] =
{
    {   // red fighter, tan skin
        0, RGB(0x181018), RGB(0xE0A878), RGB(0xB07850), RGB(0x603010),
        RGB(0xD02828), RGB(0x901818), RGB(0x2848B0), RGB(0x183078),
        RGB(0x402010), RGB(0xE0C020), RGB(0xF0F0F0), RGB(0x302838), 0, 0, 0
    },
    {   // green fighter, pale skin, blond
        0, RGB(0x181018), RGB(0xF0C8A0), RGB(0xC09870), RGB(0xE8C840),
        RGB(0x30A040), RGB(0x187028), RGB(0x505058), RGB(0x303038),
        RGB(0xD0D0D0), RGB(0xE87820), RGB(0xF0F0F0), RGB(0x302838), 0, 0, 0
    },
    {   // purple fighter, dark skin
        0, RGB(0x181018), RGB(0x8A5A38), RGB(0x5E3A22), RGB(0x181820),
        RGB(0x8838C8), RGB(0x5C2088), RGB(0xC8B890), RGB(0x988860),
        RGB(0x202020), RGB(0x20C8D0), RGB(0xF0F0F0), RGB(0x302838), 0, 0, 0
    }
};

/* ------------------------------------------------------------------ */
/* Tiny software rasterizer (48x64, 1 byte per pixel, color index)    */
/* ------------------------------------------------------------------ */
static u8 canvas[CH][CW];
static u32 tileBuf[TILES_PER_FRAME * 8];

static const s16 sinT[8] = { 0, 71, 100, 71, 0, -71, -100, -71 };
#define COST(f)  (sinT[((f) + 2) & 7])
static const u8 bobT[8] = { 2, 1, 0, 1, 2, 1, 0, 1 };

static void px(int x, int y, u8 c)
{
    if ((unsigned)x < CW && (unsigned)y < CH) canvas[y][x] = c;
}

static void rect(int x, int y, int w, int h, u8 c)
{
    int i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            px(x + i, y + j, c);
}

static void rectO(int x, int y, int w, int h, u8 c)
{
    rect(x - 1, y - 1, w + 2, h + 2, C_OL);
    rect(x, y, w, h, c);
}

static void ellipse(int cx, int cy, int rx, int ry, u8 c)
{
    int dx, dy;
    for (dy = -ry; dy <= ry; dy++)
        for (dx = -rx; dx <= rx; dx++)
            if (dx * dx * ry * ry + dy * dy * rx * rx <= rx * rx * ry * ry)
                px(cx + dx, cy + dy, c);
}

static void disc(int cx, int cy, int r, u8 c)
{
    int dx, dy;
    for (dy = -r; dy <= r; dy++)
        for (dx = -r; dx <= r; dx++)
            if (dx * dx + dy * dy <= r * r + r)
                px(cx + dx, cy + dy, c);
}

static void tline(int x0, int y0, int x1, int y1, int r, u8 c)
{
    int dx = x1 - x0, dy = y1 - y0;
    int n = IABS(dx) > IABS(dy) ? IABS(dx) : IABS(dy);
    int i;
    if (n == 0) { disc(x0, y0, r, c); return; }
    for (i = 0; i <= n; i++)
        disc(x0 + dx * i / n, y0 + dy * i / n, r, c);
}

static int my_isqrt(int v)
{
    int r = 0;
    while ((r + 1) * (r + 1) <= v) r++;
    return r;
}

// Two-bone limb with simple IK. bend=+1: knee points forward, -1: elbow points back
static void limb2(int x0, int y0, int x1, int y1, int L, int bend, int r, u8 c1, u8 c2)
{
    int dx = x1 - x0, dy = y1 - y0;
    int d2 = dx * dx + dy * dy;
    int d = my_isqrt(d2);
    int h2, h, kx, ky;
    if (d < 1) d = 1;
    h2 = L * L - d2 / 4;
    h = (h2 > 0) ? my_isqrt(h2) : 0;
    kx = (x0 + x1) / 2 + bend * dy * h / d;
    ky = (y0 + y1) / 2 - bend * dx * h / d;
    tline(x0, y0, kx, ky, r + 1, C_OL);
    tline(kx, ky, x1, y1, r + 1, C_OL);
    tline(x0, y0, kx, ky, r, c1);
    tline(kx, ky, x1, y1, r, c2);
}

static void drawLeg(int f, int far_, int v, int hipY)
{
    int s = sinT[f];
    int c = COST(f);
    int ax = 24 + s * 10 / 100;
    int lift = (c > 0) ? (c * 6 / 100) : 0;
    int ay = 57 - lift;
    u8 c1 = far_ ? C_PANTS_D : C_PANTS;
    u8 c2 = (v == 0) ? (far_ ? C_SKIN_D : C_SKIN) : c1;
    limb2(24, hipY, ax, ay, 11, +1, (v == 0) ? 3 : 2, c1, c2);
    rectO(ax - 2, ay + 1, 8, 4, C_SHOE);
}

static void drawArm(int f, int far_, int v, int shY)
{
    int s = sinT[f];
    int c = COST(f);
    int hx = 24 + s * 9 / 100;
    int hy = shY + 15 - ((c > 0) ? (c * 4 / 100) : 0);
    u8 skin = far_ ? C_SKIN_D : C_SKIN;
    u8 c1 = (v == 0) ? skin : (far_ ? C_SHIRT_D : C_SHIRT);
    limb2(24, shY, hx, hy, 9, -1, (v == 0) ? 2 : 1, c1, skin);
    disc(hx, hy, 3, C_OL);
    disc(hx, hy, 2, skin);
}

static void renderFrame(int v, int f)
{
    int bob = bobT[f];
    int hipY = 38 - bob;
    int shY = 23 - bob;
    int fA = f, fB = (f + 4) & 7;
    int w = (v == 0) ? 18 : 14;
    int x0 = 24 - w / 2;
    int ty = 19 - bob;
    int hx = 26, hy = 12 - bob;
    int dx, dy;

    memset(canvas, 0, sizeof(canvas));

    // ground shadow
    ellipse(24, 61, 15, 2, C_SHADOW);

    // far limbs
    drawLeg(fB, 1, v, hipY);
    drawArm(fA, 1, v, shY);

    // torso
    rectO(x0, ty, w, 19, C_SHIRT);
    rect(x0, ty, 4, 19, C_SHIRT_D);
    if (v == 0) rect(x0 + w - 8, ty + 5, 4, 4, C_WHITE);       // chest logo
    else        rect(x0 + w - 5, ty, 2, 19, C_BELT);           // jacket stripe

    // pelvis + belt
    rectO(x0, hipY - 4, w, 7, C_PANTS);
    rect(x0, hipY - 2, 3, 5, C_PANTS_D);
    rect(x0, hipY - 4, w, 2, C_BELT);

    // near leg
    drawLeg(fA, 0, v, hipY);

    // ponytail (behind head)
    if (v == 1)
    {
        int tx = hx - 10 + sinT[(f + 1) & 7] * 3 / 100;
        tline(hx - 6, hy - 3, tx, hy + 8, 3, C_OL);
        tline(hx - 6, hy - 3, tx, hy + 8, 2, C_HAIR);
    }

    // head
    ellipse(hx, hy, 8, 9, C_OL);
    for (dy = -8; dy <= 8; dy++)
    {
        for (dx = -7; dx <= 7; dx++)
        {
            if (dx * dx * 64 + dy * dy * 49 <= 49 * 64)
            {
                u8 c = C_SKIN;
                if (v == 0)
                {
                    if (dy <= -4) c = C_HAIR;
                    else if (dx <= -4 && dy <= 3) c = C_HAIR;
                    if (dy == -3 || dy == -2) c = C_BELT;      // headband
                }
                else
                {
                    if (dy <= -4 || (dx <= -3 && dy <= 5)) c = C_HAIR;
                    if (dy <= -3 && dx >= 2) c = C_HAIR;       // fringe
                }
                px(hx + dx, hy + dy, c);
            }
        }
    }
    px(hx + 3, hy - 1, C_WHITE);
    px(hx + 4, hy - 1, C_OL);
    px(hx + 6, hy + 1, C_SKIN_D);
    px(hx + 4, hy + 4, C_OL);
    px(hx + 5, hy + 4, C_OL);

    // near arm
    drawArm(fB, 0, v, shY);
}

// Pack part of the canvas into a hardware-sprite tile order (column-major)
static void packPiece(int colStart, int rowStart, int wT, int hT, int tileOffset)
{
    int c, r, y, x;
    for (c = 0; c < wT; c++)
    {
        for (r = 0; r < hT; r++)
        {
            u32 *t = &tileBuf[(tileOffset + c * hT + r) * 8];
            for (y = 0; y < 8; y++)
            {
                const u8 *src = &canvas[(rowStart + r) * 8 + y][(colStart + c) * 8];
                u32 val = 0;
                for (x = 0; x < 8; x++) val = (val << 4) | (src[x] & 15);
                *t++ = val;
            }
        }
    }
}

static void buildCharacterTiles(void)
{
    int v, f;
    for (v = 0; v < NUM_VARIANTS; v++)
    {
        for (f = 0; f < FRAMES; f++)
        {
            renderFrame(v, f);
            packPiece(0, 0, 4, 4, 0);     // top-left     32x32
            packPiece(4, 0, 2, 4, 16);    // top-right    16x32
            packPiece(0, 4, 4, 4, 24);    // bottom-left  32x32
            packPiece(4, 4, 2, 4, 40);    // bottom-right 16x32
            VDP_loadTileData(tileBuf,
                             SPR_TILE_BASE + v * TILES_PER_VARIANT + f * TILES_PER_FRAME,
                             TILES_PER_FRAME, CPU);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Background (street scene) tiles                                    */
/* ------------------------------------------------------------------ */
enum { T_BRICK, T_WTL, T_WTR, T_WBL, T_WBR, T_LEDGE, T_SIDE, T_CURB, T_ASPH, T_LANE, T_COUNT };

static const u16 bgPal[16] =
{
    RGB(0x181018), RGB(0x483838), RGB(0xA04838), RGB(0x803028),
    RGB(0xE8E0D0), RGB(0x3858A0), RGB(0x80B0E0), RGB(0xB0B0B8),
    RGB(0x808088), RGB(0x484850), RGB(0x383840), RGB(0xE8C020),
    RGB(0x988878), RGB(0xE8E8E8), RGB(0x202020), RGB(0xFFFFFF)
};

static u8 bgPixel(int t, int x, int y)
{
    switch (t)
    {
        case T_BRICK:
            if (y == 3 || y == 7) return 1;
            if (y < 3 && x == 0) return 1;
            if (y > 3 && x == 4) return 1;
            return (((x * 7 + y * 13) & 7) == 0) ? 3 : 2;

        case T_WTL: case T_WTR: case T_WBL: case T_WBR:
        {
            int gx = x + ((t == T_WTR || t == T_WBR) ? 8 : 0);
            int gy = y + ((t == T_WBL || t == T_WBR) ? 8 : 0);
            if (gx < 2 || gx > 13 || gy < 2 || gy > 13) return 4;
            if (gx == 7 || gx == 8 || gy == 7 || gy == 8) return 4;
            return (((gx + gy) & 7) == 0) ? 6 : 5;
        }

        case T_LEDGE:
            if (y < 2) return 13;
            if (y < 6) return 12;
            return 1;

        case T_SIDE:
            if (x == 7 || y == 7) return 8;
            return (((x * 5 + y * 3) & 15) == 0) ? 8 : 7;

        case T_CURB:
            if (y < 2) return 13;
            if (y < 6) return 7;
            return 8;

        case T_LANE:
            if (y == 3 || y == 4) return 11;
            /* fall through */
        case T_ASPH:
        default:
            return (((x * 13 + y * 7) & 15) == 0) ? 10 : 9;
    }
}

static void buildBackground(void)
{
    int t, x, y, bx, by;
    u32 buf[8];

    for (t = 0; t < T_COUNT; t++)
    {
        for (y = 0; y < 8; y++)
        {
            u32 val = 0;
            for (x = 0; x < 8; x++) val = (val << 4) | bgPixel(t, x, y);
            buf[y] = val;
        }
        VDP_loadTileData(buf, BG_TILE_BASE + t, 1, CPU);
    }

    #define PUT(X, Y, T) VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, BG_TILE_BASE + (T)), (X), (Y))

    for (y = 0; y < 28; y++)
    {
        for (x = 0; x < 40; x++)
        {
            int tile;
            if (y < 13)       tile = T_BRICK;
            else if (y == 13) tile = T_LEDGE;
            else if (y < 18)  tile = T_SIDE;
            else if (y == 18) tile = T_CURB;
            else if (y == 24) tile = ((x & 3) < 2) ? T_LANE : T_ASPH;
            else              tile = T_ASPH;
            PUT(x, y, tile);
        }
    }
    for (bx = 2; bx <= 34; bx += 6)
    {
        for (by = 2; by <= 7; by += 5)
        {
            PUT(bx,     by,     T_WTL);
            PUT(bx + 1, by,     T_WTR);
            PUT(bx,     by + 1, T_WBL);
            PUT(bx + 1, by + 1, T_WBR);
        }
    }
    #undef PUT
}

/* ------------------------------------------------------------------ */
/* Actors                                                             */
/* ------------------------------------------------------------------ */
typedef struct
{
    s16 x, y;           // feet-center position, 1/16 pixel units
    s16 vx;             // NPC horizontal speed (1/16 px per frame)
    s16 minX, maxX;     // NPC patrol bounds in pixels
    u8  frame, timer;   // own walk-cycle state
    u8  variant, pal;
    u8  face;           // 0 = right, 1 = left
} Actor;

static Actor actors[NUM_ACTORS];

#define PLAYER_SPEED_X   20
#define PLAYER_SPEED_Y   16
#define MIN_FEET_Y       160
#define MAX_FEET_Y       222

static void initActor(Actor *a, u8 variant, u8 pal, int x, int y, int vx,
                      int minX, int maxX, u8 frame, u8 timer)
{
    a->x = x * 16;
    a->y = y * 16;
    a->vx = vx;
    a->minX = minX;
    a->maxX = maxX;
    a->frame = frame;
    a->timer = timer;
    a->variant = variant;
    a->pal = pal;
    a->face = (vx < 0) ? 1 : 0;
}

static void animate(Actor *a)
{
    if (++a->timer >= ANIM_PERIOD)
    {
        a->timer = 0;
        a->frame = (a->frame + 1) & 7;
    }
}

static void updatePlayer(Actor *p)
{
    u16 j = JOY_readJoypad(JOY_1);
    int moving = 0;

    if (j & BUTTON_LEFT)       { p->x -= PLAYER_SPEED_X; p->face = 1; moving = 1; }
    else if (j & BUTTON_RIGHT) { p->x += PLAYER_SPEED_X; p->face = 0; moving = 1; }
    if (j & BUTTON_UP)         { p->y -= PLAYER_SPEED_Y; moving = 1; }
    else if (j & BUTTON_DOWN)  { p->y += PLAYER_SPEED_Y; moving = 1; }

    if (p->x < 24 * 16)  p->x = 24 * 16;
    if (p->x > 296 * 16) p->x = 296 * 16;
    if (p->y < MIN_FEET_Y * 16) p->y = MIN_FEET_Y * 16;
    if (p->y > MAX_FEET_Y * 16) p->y = MAX_FEET_Y * 16;

    if (moving) animate(p);
    else        p->timer = 0;
}

static void updateNpc(Actor *a)
{
    a->x += a->vx;
    if ((a->x >> 4) >= a->maxX) { a->x = a->maxX * 16; a->vx = -IABS(a->vx); }
    if ((a->x >> 4) <= a->minX) { a->x = a->minX * 16; a->vx =  IABS(a->vx); }
    a->face = (a->vx < 0) ? 1 : 0;
    animate(a);
}

static void drawActors(void)
{
    u8 order[NUM_ACTORS];
    int i, j, s;

    // sort by feet Y, descending: nearer fighters get lower sprite index (drawn on top)
    for (i = 0; i < NUM_ACTORS; i++) order[i] = i;
    for (i = 1; i < NUM_ACTORS; i++)
    {
        u8 k = order[i];
        j = i - 1;
        while (j >= 0 && actors[order[j]].y < actors[k].y)
        {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = k;
    }

    for (s = 0; s < NUM_ACTORS; s++)
    {
        const Actor *a = &actors[order[s]];
        int ox = (a->x >> 4) - 24;
        int oy = (a->y >> 4) - 63;
        u16 base = SPR_TILE_BASE + a->variant * TILES_PER_VARIANT + a->frame * TILES_PER_FRAME;
        u16 flip = a->face;
        int xWide   = flip ? ox + 16 : ox;        // 32-px wide piece
        int xNarrow = flip ? ox      : ox + 32;   // 16-px wide piece
        u16 idx = s * 4;
        u16 lastLink = (s == NUM_ACTORS - 1) ? 0 : idx + 4;

        VDP_setSpriteFull(idx + 0, xWide,   oy,      SPRITE_SIZE(4, 4),
                          TILE_ATTR_FULL(a->pal, FALSE, FALSE, flip, base + 0),  idx + 1);
        VDP_setSpriteFull(idx + 1, xNarrow, oy,      SPRITE_SIZE(2, 4),
                          TILE_ATTR_FULL(a->pal, FALSE, FALSE, flip, base + 16), idx + 2);
        VDP_setSpriteFull(idx + 2, xWide,   oy + 32, SPRITE_SIZE(4, 4),
                          TILE_ATTR_FULL(a->pal, FALSE, FALSE, flip, base + 24), idx + 3);
        VDP_setSpriteFull(idx + 3, xNarrow, oy + 32, SPRITE_SIZE(2, 4),
                          TILE_ATTR_FULL(a->pal, FALSE, FALSE, flip, base + 40), lastLink);
    }
    VDP_updateSprites(NUM_ACTORS * 4, DMA_QUEUE);
}

/* ------------------------------------------------------------------ */
int main(bool hardReset)
{
    (void)hardReset;

    VDP_setScreenWidth320();
    VDP_setEnable(FALSE);

    PAL_setPalette(PAL0, bgPal, CPU);
    PAL_setPalette(PAL1, charPal[0], CPU);
    PAL_setPalette(PAL2, charPal[1], CPU);
    PAL_setPalette(PAL3, charPal[2], CPU);

    buildBackground();
    buildCharacterTiles();

    VDP_drawText("D-PAD: MOVE THE RED FIGHTER", 1, 1);

    //            variant pal  x    y    vx  minX maxX frame timer
    initActor(&actors[0], 0, PAL1, 160, 190,   0,   0,   0, 0, 0);   // player
    initActor(&actors[1], 1, PAL2,  60, 170,  12,  40, 150, 3, 2);
    initActor(&actors[2], 0, PAL3, 250, 200, -14, 150, 290, 6, 4);
    initActor(&actors[3], 1, PAL1, 100, 214,  16,  30, 280, 1, 5);
    initActor(&actors[4], 0, PAL2, 220, 182, -10, 120, 300, 5, 1);

    VDP_resetSprites();
    VDP_setEnable(TRUE);

    while (TRUE)
    {
        int i;
        updatePlayer(&actors[0]);
        for (i = 1; i < NUM_ACTORS; i++) updateNpc(&actors[i]);

        drawActors();
        SYS_doVBlankProcess();
    }
    return 0;
}
