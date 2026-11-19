/*
 * dialog.c -> main.c : Sega Master System dialog box demo
 *
 * - Framed dialog box on the bottom 6 tile rows (rows 18..23).
 * - Text is typed out one character per frame, then held, then the box is
 *   erased (one row per frame) and the next message starts.
 * - A 16x16 character sprite walks back and forth above the box (2 walk frames).
 * - All tiles and palettes are C arrays in this file.
 *
 * Build: sdcc -c -mz80 main.c
 *        (link with crt0_sms.rel and SMSlib.lib, then makesms)
 *
 * Tile art format (all tiles): text strings, 8 rows of 8 characters:
 *   ' ' = palette index 0    '+' = palette index 1
 *   '.' = palette index 2    '#' = palette index 3
 * The loader packs these into SMS 4bpp tile data (32 bytes per tile).
 *
 * All VRAM writes happen right after SMS_waitForVBlank(), inside the
 * vertical blanking period, so there is no screen corruption on real hardware.
 */

#include "SMSlib.h"

/* ------------------------------------------------------------------ */
/* Layout (in tiles)                                                  */
/* ------------------------------------------------------------------ */
#define BOX_X0          1
#define BOX_X1          30
#define BOX_Y0          18
#define BOX_Y1          23

#define IN_X0           2
#define IN_X1           29
#define IN_Y0           19
#define IN_Y1           22

#define TEXT_X          3
#define TEXT_MAX_X      28

/* ------------------------------------------------------------------ */
/* Background tile indices (VRAM tiles 0..37)                         */
/* ------------------------------------------------------------------ */
#define T_BLANK         0
#define T_FILL          1
#define T_TL            2
#define T_T             3
#define T_TR            4
#define T_L             5
#define T_R             6
#define T_BL            7
#define T_B             8
#define T_BR            9
#define T_GLYPH0        10   /* '.', '!', 'A'..'Z' follow */
#define NUM_BG_TILES    38

/* ------------------------------------------------------------------ */
/* Sprites                                                            */
/* ------------------------------------------------------------------ */
#define SPR_TILE0       256  /* sprites use the second half of VRAM tiles */
#define WALK_Y          120
#define WALK_MIN_X      8
#define WALK_MAX_X      232

/* ------------------------------------------------------------------ */
/* Dialog                                                             */
/* ------------------------------------------------------------------ */
#define HOLD_FRAMES     150
#define NUM_MESSAGES    4

#define PH_TYPE         0
#define PH_HOLD         1
#define PH_CLEAR        2

/* ------------------------------------------------------------------ */
/* Palettes (SMS CRAM format: 00BBGGRR, 2 bits per channel)           */
/* ------------------------------------------------------------------ */
static const unsigned char bg_palette[16] = {
    0x15,   /* 0: backdrop (dark gray)      */
    0x0F,   /* 1: frame (yellow)            */
    0x20,   /* 2: box fill (dark blue)      */
    0x3F,   /* 3: text ink (white)          */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const unsigned char spr_palette[16] = {
    0x00,   /* 0: transparent               */
    0x03,   /* 1: hair / belt (red)         */
    0x1B,   /* 2: skin (peach)              */
    0x0C,   /* 3: shirt (green)             */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* ------------------------------------------------------------------ */
/* Background tiles: 38 tiles x 8 rows x 8 chars                      */
/* ------------------------------------------------------------------ */
static const char tiles_art[] =
    /* 0  T_BLANK */
    "        " "        " "        " "        " "        " "        " "        " "        "
    /* 1  T_FILL */
    "........" "........" "........" "........" "........" "........" "........" "........"
    /* 2  T_TL */
    "++++++++" "+......." "+......." "+......." "+......." "+......." "+......." "+......."
    /* 3  T_T */
    "++++++++" "........" "........" "........" "........" "........" "........" "........"
    /* 4  T_TR */
    "++++++++" ".......+" ".......+" ".......+" ".......+" ".......+" ".......+" ".......+"
    /* 5  T_L */
    "+......." "+......." "+......." "+......." "+......." "+......." "+......." "+......."
    /* 6  T_R */
    ".......+" ".......+" ".......+" ".......+" ".......+" ".......+" ".......+" ".......+"
    /* 7  T_BL */
    "+......." "+......." "+......." "+......." "+......." "+......." "+......." "++++++++"
    /* 8  T_B */
    "........" "........" "........" "........" "........" "........" "........" "++++++++"
    /* 9  T_BR */
    ".......+" ".......+" ".......+" ".......+" ".......+" ".......+" ".......+" "++++++++"
    /* 10 '.' */
    "........" "........" "........" "........" "........" "..#....." "........" "........"
    /* 11 '!' */
    "..#....." "..#....." "..#....." "..#....." "..#....." "........" "..#....." "........"
    /* 12 'A' */
    ".###...." "#...#..." "#...#..." "#####..." "#...#..." "#...#..." "#...#..." "........"
    /* 13 'B' */
    "####...." "#...#..." "#...#..." "####...." "#...#..." "#...#..." "####...." "........"
    /* 14 'C' */
    ".####..." "#......." "#......." "#......." "#......." "#......." ".####..." "........"
    /* 15 'D' */
    "####...." "#...#..." "#...#..." "#...#..." "#...#..." "#...#..." "####...." "........"
    /* 16 'E' */
    "#####..." "#......." "#......." "####...." "#......." "#......." "#####..." "........"
    /* 17 'F' */
    "#####..." "#......." "#......." "####...." "#......." "#......." "#......." "........"
    /* 18 'G' */
    ".####..." "#......." "#......." "#.###..." "#...#..." "#...#..." ".####..." "........"
    /* 19 'H' */
    "#...#..." "#...#..." "#...#..." "#####..." "#...#..." "#...#..." "#...#..." "........"
    /* 20 'I' */
    "#####..." "..#....." "..#....." "..#....." "..#....." "..#....." "#####..." "........"
    /* 21 'J' */
    "..###..." "...#...." "...#...." "...#...." "#..#...." "#..#...." ".##....." "........"
    /* 22 'K' */
    "#...#..." "#..#...." "#.#....." "##......" "#.#....." "#..#...." "#...#..." "........"
    /* 23 'L' */
    "#......." "#......." "#......." "#......." "#......." "#......." "#####..." "........"
    /* 24 'M' */
    "#...#..." "##.##..." "#.#.#..." "#.#.#..." "#...#..." "#...#..." "#...#..." "........"
    /* 25 'N' */
    "#...#..." "##..#..." "#.#.#..." "#..##..." "#...#..." "#...#..." "#...#..." "........"
    /* 26 'O' */
    ".###...." "#...#..." "#...#..." "#...#..." "#...#..." "#...#..." ".###...." "........"
    /* 27 'P' */
    "####...." "#...#..." "#...#..." "####...." "#......." "#......." "#......." "........"
    /* 28 'Q' */
    ".###...." "#...#..." "#...#..." "#...#..." "#.#.#..." "#..#...." ".##.#..." "........"
    /* 29 'R' */
    "####...." "#...#..." "#...#..." "####...." "#.#....." "#..#...." "#...#..." "........"
    /* 30 'S' */
    ".####..." "#......." "#......." ".###...." "....#..." "....#..." "####...." "........"
    /* 31 'T' */
    "#####..." "..#....." "..#....." "..#....." "..#....." "..#....." "..#....." "........"
    /* 32 'U' */
    "#...#..." "#...#..." "#...#..." "#...#..." "#...#..." "#...#..." ".###...." "........"
    /* 33 'V' */
    "#...#..." "#...#..." "#...#..." "#...#..." "#...#..." ".#.#...." "..#....." "........"
    /* 34 'W' */
    "#...#..." "#...#..." "#...#..." "#.#.#..." "#.#.#..." "##.##..." "#...#..." "........"
    /* 35 'X' */
    "#...#..." "#...#..." ".#.#...." "..#....." ".#.#...." "#...#..." "#...#..." "........"
    /* 36 'Y' */
    "#...#..." "#...#..." ".#.#...." "..#....." "..#....." "..#....." "..#....." "........"
    /* 37 'Z' */
    "#####..." "....#..." "...#...." "..#....." ".#......" "#......." "#####..." "........"
;

/* ------------------------------------------------------------------ */
/* Character sprite: two 16x16 walk frames, 16 rows x 16 chars each   */
/* Frame 0 = legs together, frame 1 = legs apart                       */
/* ------------------------------------------------------------------ */
static const char sprite_art[] =
    /* frame 0 */
    "     ++++++     "
    "    ++++++++    "
    "    +......+    "
    "    +.+..+.+    "
    "    +......+    "
    "     ++++++     "
    "  ############  "
    "  .##########.  "
    "  .##########.  "
    "  ++++++++++++  "
    "   ##########   "
    "   ##########   "
    "     ++++++     "
    "     ++  ++     "
    "     ++  ++     "
    "    ++    ++    "
    /* frame 1 */
    "     ++++++     "
    "    ++++++++    "
    "    +......+    "
    "    +.+..+.+    "
    "    +......+    "
    "     ++++++     "
    "  ############  "
    "  .##########.  "
    "  .##########.  "
    "  ++++++++++++  "
    "   ##########   "
    "   ##########   "
    "     ++++++     "
    "    ++    ++    "
    "    ++    ++    "
    "  +++    +++    "
;

/* ------------------------------------------------------------------ */
/* Dialog text (uppercase, '\n' = new line, max 26 chars per line)    */
/* ------------------------------------------------------------------ */
static const char *const messages[NUM_MESSAGES] = {
    "HELLO! WELCOME TO THE\nSEGA MASTER SYSTEM.",
    "THIS TEXT IS DRAWN ONE\nCHARACTER AT A TIME.",
    "THEN THE BOX IS CLEARED\nAND THE NEXT ONE BEGINS.",
    "THANKS FOR WATCHING!\nSEE YOU AGAIN SOON!"
};

/* ------------------------------------------------------------------ */
/* State                                                              */
/* ------------------------------------------------------------------ */
static unsigned char msg_index;
static unsigned char char_index;
static unsigned char cur_x;
static unsigned char cur_y;
static unsigned char phase;
static unsigned char timer;
static unsigned char clear_y;

static int           walk_x;
static signed char   walk_dir;
static unsigned char walk_anim;
static unsigned char walk_frame;

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

/* maps an art character to a 2bpp color index */
static unsigned char art_color(char c)
{
    switch (c) {
        case '+': return 1;
        case '.': return 2;
        case '#': return 3;
        default:  return 0;
    }
}

/* packs one 8x8 art tile (rows separated by 'stride' chars) into
 * SMS 4bpp format and writes it to VRAM tile 'vram_tile' */
static void load_art_tile(const char *art, unsigned char stride, unsigned int vram_tile)
{
    unsigned char buf[32];
    unsigned char row, col, idx, p0, p1, bit;

    for (row = 0; row < 8; row++) {
        p0 = 0;
        p1 = 0;
        for (col = 0; col < 8; col++) {
            idx = art_color(art[row * stride + col]);
            bit = (unsigned char)(0x80 >> col);
            if (idx & 1) p0 |= bit;
            if (idx & 2) p1 |= bit;
        }
        /* 4bpp row: plane0, plane1, plane2 (unused), plane3 (unused) */
        buf[row * 4 + 0] = p0;
        buf[row * 4 + 1] = p1;
        buf[row * 4 + 2] = 0;
        buf[row * 4 + 3] = 0;
    }
    SMS_loadTiles(buf, vram_tile, 32);
}

/* returns the tile for the box cell at (x,y) */
static unsigned char box_tile(unsigned char x, unsigned char y)
{
    if (y == BOX_Y0) {
        if (x == BOX_X0) return T_TL;
        if (x == BOX_X1) return T_TR;
        return T_T;
    }
    if (y == BOX_Y1) {
        if (x == BOX_X0) return T_BL;
        if (x == BOX_X1) return T_BR;
        return T_B;
    }
    if (x == BOX_X0) return T_L;
    if (x == BOX_X1) return T_R;
    return T_FILL;
}

/* maps a character to a background tile */
static unsigned char glyph_tile(char c)
{
    if (c >= 'a' && c <= 'z') c = (char)(c - 32);

    if (c == ' ')  return T_FILL;
    if (c == '.')  return T_GLYPH0 + 0;
    if (c == '!')  return T_GLYPH0 + 1;
    if (c >= 'A' && c <= 'Z') return (unsigned char)(T_GLYPH0 + 2 + (c - 'A'));
    return T_FILL;
}

/* ------------------------------------------------------------------ */
/* Setup                                                              */
/* ------------------------------------------------------------------ */
static void init_video(void)
{
    unsigned int i;
    unsigned char qx, qy, f;

    SMS_displayOff();
    SMS_useFirstHalfTilesforSprites(0);   /* sprites use VRAM tiles 256..511 */

    SMS_loadBGPalette(bg_palette);
    SMS_loadSpritePalette(spr_palette);

    /* background tiles */
    for (i = 0; i < NUM_BG_TILES; i++)
        load_art_tile(tiles_art + i * 64, 8, i);

    /* sprite tiles: 2 frames x 4 quadrants (TL, TR, BL, BR) */
    for (f = 0; f < 2; f++) {
        for (qy = 0; qy < 2; qy++) {
            for (qx = 0; qx < 2; qx++) {
                load_art_tile(sprite_art + f * 256 + qy * 128 + qx * 8, 16,
                              SPR_TILE0 + f * 4 + qy * 2 + qx);
            }
        }
    }

    /* clear the whole name table to the blank tile */
    SMS_setNextTileatXY(0, 0);
    for (i = 0; i < 768; i++)
        SMS_setTile(T_BLANK);

    SMS_initSprites();
}

static void draw_box(void)
{
    unsigned char x, y;

    for (y = BOX_Y0; y <= BOX_Y1; y++)
        for (x = BOX_X0; x <= BOX_X1; x++)
            SMS_setTileatXY(x, y, box_tile(x, y));
}

static void start_message(void)
{
    char_index = 0;
    cur_x = TEXT_X;
    cur_y = IN_Y0;
    phase = PH_TYPE;
}

/* ------------------------------------------------------------------ */
/* Dialog: called once per frame, right after the VBlank wait         */
/* ------------------------------------------------------------------ */
static void dialog_step(void)
{
    char c;
    unsigned char i;

    switch (phase) {

    case PH_TYPE:
        c = messages[msg_index][char_index];
        if (c == '\0') {
            phase = PH_HOLD;
            timer = HOLD_FRAMES;
        } else {
            char_index++;
            if (c == '\n') {
                cur_x = TEXT_X;
                cur_y++;
            } else {
                if (cur_x <= TEXT_MAX_X && cur_y <= IN_Y1)
                    SMS_setTileatXY(cur_x, cur_y, glyph_tile(c));
                cur_x++;
            }
        }
        break;

    case PH_HOLD:
        timer--;
        if (timer == 0) {
            phase = PH_CLEAR;
            clear_y = IN_Y0;
        }
        break;

    case PH_CLEAR:
        /* erase one interior row per frame */
        SMS_setNextTileatXY(IN_X0, clear_y);
        for (i = IN_X0; i <= IN_X1; i++)
            SMS_setTile(T_FILL);
        clear_y++;
        if (clear_y > IN_Y1) {
            msg_index++;
            if (msg_index >= NUM_MESSAGES)
                msg_index = 0;
            start_message();
        }
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Walking character (sprites, RAM-only work)                         */
/* ------------------------------------------------------------------ */
static void walker_init(void)
{
    walk_x = WALK_MIN_X;
    walk_dir = 1;
    walk_anim = 0;
    walk_frame = 0;
}

static void walker_update(void)
{
    walk_x += walk_dir;
    if (walk_x <= WALK_MIN_X) walk_dir = 1;
    if (walk_x >= WALK_MAX_X) walk_dir = -1;

    walk_anim++;
    if (walk_anim >= 8) {
        walk_anim = 0;
        walk_frame ^= 1;
    }
}

static void walker_draw(void)
{
    unsigned char base = walk_frame ? 4 : 0;
    int x = walk_x;

    SMS_addSprite(x,     WALK_Y,     base + 0);
    SMS_addSprite(x + 8, WALK_Y,     base + 1);
    SMS_addSprite(x,     WALK_Y + 8, base + 2);
    SMS_addSprite(x + 8, WALK_Y + 8, base + 3);
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */
void main(void)
{
    init_video();
    draw_box();

    msg_index = 0;
    start_message();
    walker_init();

    SMS_displayOn();

    while (1) {
        SMS_waitForVBlank();

        /* VRAM work: only inside the vertical blank */
        SMS_copySpritestoSAT();
        dialog_step();

        /* RAM-only work for the next frame */
        walker_update();
        SMS_initSprites();
        walker_draw();
    }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
