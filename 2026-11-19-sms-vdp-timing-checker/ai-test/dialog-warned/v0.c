#include "SMSlib.h"

/* ------------------------------------------------------------------ */
/* Tile layout in VRAM (sprites also use the first 256 tiles)         */
/*   0..30  font (space, A-Z, . ! , ?)                                */
/*   31..38 frame tiles                                               */
/*   39..44 character sprite tiles                                    */
/* ------------------------------------------------------------------ */
#define TILE_FONT     0
#define TILE_FRAME    31
#define TILE_SPRITE   39

#define FR_TL    (TILE_FRAME + 0)
#define FR_TR    (TILE_FRAME + 1)
#define FR_BL    (TILE_FRAME + 2)
#define FR_BR    (TILE_FRAME + 3)
#define FR_HTOP  (TILE_FRAME + 4)
#define FR_HBOT  (TILE_FRAME + 5)
#define FR_VL    (TILE_FRAME + 6)
#define FR_VR    (TILE_FRAME + 7)

#define BOX_TOP_ROW   18
#define BOX_BOT_ROW   23
#define BOX_LEFT_COL  1
#define BOX_RIGHT_COL 30
#define TEXT_COL      3
#define TEXT_ROW      19
#define TEXT_LINES    4
#define CLEAR_COL0    2
#define CLEAR_COL1    29

#define SPRITE_Y      112
#define HOLD_FRAMES   90

/* ------------------------------------------------------------------ */
/* Palettes (6-bit BBGGRR)                                            */
/* ------------------------------------------------------------------ */
const unsigned char bg_palette[16] = {
  RGB(0,0,1), RGB(3,3,3), RGB(3,3,0), RGB(2,2,2),
  RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0),
  RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0),
  RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0)
};

const unsigned char spr_palette[16] = {
  RGB(0,0,1), RGB(3,2,1), RGB(0,2,3), RGB(1,0,0),
  RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0),
  RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0),
  RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0)
};

/* ------------------------------------------------------------------ */
/* 1bpp font: space, A-Z, '.', '!', ',', '?'                           */
/* ------------------------------------------------------------------ */
const unsigned char font_1bpp[31 * 8] = {
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* space */
  0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0x00, /* A */
  0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00, /* B */
  0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00, /* C */
  0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00, /* D */
  0x7E,0x60,0x60,0x78,0x60,0x60,0x7E,0x00, /* E */
  0x7E,0x60,0x60,0x78,0x60,0x60,0x60,0x00, /* F */
  0x3C,0x66,0x60,0x6E,0x66,0x66,0x3E,0x00, /* G */
  0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00, /* H */
  0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00, /* I */
  0x1E,0x0C,0x0C,0x0C,0x0C,0x6C,0x38,0x00, /* J */
  0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00, /* K */
  0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00, /* L */
  0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00, /* M */
  0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00, /* N */
  0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00, /* O */
  0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00, /* P */
  0x3C,0x66,0x66,0x66,0x6A,0x6C,0x36,0x00, /* Q */
  0x7C,0x66,0x66,0x7C,0x6C,0x66,0x66,0x00, /* R */
  0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00, /* S */
  0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00, /* T */
  0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00, /* U */
  0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00, /* V */
  0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00, /* W */
  0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00, /* X */
  0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00, /* Y */
  0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00, /* Z */
  0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00, /* . */
  0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00, /* ! */
  0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30, /* , */
  0x3C,0x66,0x06,0x0C,0x18,0x00,0x18,0x00  /* ? */
};

/* ------------------------------------------------------------------ */
/* 1bpp frame tiles: TL, TR, BL, BR, top, bottom, left, right          */
/* ------------------------------------------------------------------ */
const unsigned char frame_1bpp[8 * 8] = {
  0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x03, /* TL */
  0x00,0x00,0x00,0x00,0x00,0x00,0xC0,0xC0, /* TR */
  0x03,0x03,0x00,0x00,0x00,0x00,0x00,0x00, /* BL */
  0xC0,0xC0,0x00,0x00,0x00,0x00,0x00,0x00, /* BR */
  0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0xFF, /* top */
  0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00, /* bottom */
  0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03, /* left */
  0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0  /* right */
};

/* ------------------------------------------------------------------ */
/* 4bpp sprite tiles (planar: 4 bytes per row, plane0..plane3)        */
/* colors: 1 = skin, 2 = tunic, 3 = dark                              */
/* order: TL, TR, BL(A), BR(A), BL(B), BR(B)                          */
/* ------------------------------------------------------------------ */
#define R(p0,p1) (p0),(p1),0x00,0x00

const unsigned char sprite_tiles[6 * 32] = {
  /* TL */
  R(0x00,0x00), R(0x3F,0x3F), R(0x7F,0x7F), R(0x7F,0x60),
  R(0x7F,0x48), R(0x7F,0x40), R(0x3F,0x20), R(0x00,0x3F),
  /* TR */
  R(0x00,0x00), R(0xFC,0xFC), R(0xFE,0xFE), R(0xFE,0x06),
  R(0xFE,0x12), R(0xFE,0x02), R(0xFC,0x04), R(0x00,0xFC),
  /* BL, legs apart */
  R(0x00,0x7F), R(0x80,0x7F), R(0x80,0x7F), R(0x00,0x7F),
  R(0x3F,0x3F), R(0x3C,0x3C), R(0x3C,0x3C), R(0x3C,0x3C),
  /* BR, legs apart */
  R(0x00,0xFE), R(0x01,0xFE), R(0x01,0xFE), R(0x00,0xFE),
  R(0xFC,0xFC), R(0x3C,0x3C), R(0x3C,0x3C), R(0x3C,0x3C),
  /* BL, legs together */
  R(0x00,0x7F), R(0x80,0x7F), R(0x80,0x7F), R(0x00,0x7F),
  R(0x3F,0x3F), R(0x0F,0x0F), R(0x0F,0x0F), R(0x0F,0x0F),
  /* BR, legs together */
  R(0x00,0xFE), R(0x01,0xFE), R(0x01,0xFE), R(0x00,0xFE),
  R(0xFC,0xFC), R(0xF0,0xF0), R(0xF0,0xF0), R(0xF0,0xF0)
};

/* ------------------------------------------------------------------ */
/* Messages ('\n' = new line, max 26 chars per line, max 4 lines)     */
/* ------------------------------------------------------------------ */
#define NUM_MESSAGES 4
const char *const messages[NUM_MESSAGES] = {
  "HELLO, TRAVELER!\nWELCOME TO THE LAND\nOF SEGA.",
  "THE ROAD AHEAD IS LONG.\nBRING SUPPLIES AND\nDO NOT FEAR THE DARK.",
  "BEWARE THE CAVE TO THE\nNORTH. MANY WHO ENTER\nNEVER RETURN.",
  "GOOD LUCK, BRAVE HERO!"
};

/* ------------------------------------------------------------------ */
/* State                                                              */
/* ------------------------------------------------------------------ */
#define ST_TYPING 0
#define ST_HOLD   1
#define ST_CLEAR  2

static unsigned char state;
static unsigned char msg_index;
static const char *msg_ptr;
static unsigned char cur_x;
static unsigned char cur_y;
static unsigned char hold_timer;
static unsigned char clear_row;

static unsigned char sprite_x;
static signed char sprite_dir;
static unsigned char anim_timer;
static unsigned char anim_frame;

static unsigned char char_to_tile (char c) {
  if (c >= 'A' && c <= 'Z') return (unsigned char)(TILE_FONT + 1 + (c - 'A'));
  if (c == '.') return TILE_FONT + 27;
  if (c == '!') return TILE_FONT + 28;
  if (c == ',') return TILE_FONT + 29;
  if (c == '?') return TILE_FONT + 30;
  return TILE_FONT;
}

/* Called only with display off */
static void draw_frame (void) {
  unsigned char x, y;
  unsigned int i;

  /* clear the entire name table with the blank tile */
  SMS_setNextTileatXY(0, 0);
  for (i = 0; i < 32u * 28u; i++) SMS_setTile(TILE_FONT);

  /* top border */
  SMS_setTileatXY(BOX_LEFT_COL, BOX_TOP_ROW, FR_TL);
  for (x = BOX_LEFT_COL + 1; x < BOX_RIGHT_COL; x++) SMS_setTile(FR_HTOP);
  SMS_setTile(FR_TR);

  /* sides */
  for (y = BOX_TOP_ROW + 1; y < BOX_BOT_ROW; y++) {
    SMS_setTileatXY(BOX_LEFT_COL, y, FR_VL);
    SMS_setTileatXY(BOX_RIGHT_COL, y, FR_VR);
  }

  /* bottom border */
  SMS_setTileatXY(BOX_LEFT_COL, BOX_BOT_ROW, FR_BL);
  for (x = BOX_LEFT_COL + 1; x < BOX_RIGHT_COL; x++) SMS_setTile(FR_HBOT);
  SMS_setTile(FR_BR);
}

/* Must be called during VBlank (short: 28 tiles) */
static void clear_box_row (unsigned char row) {
  unsigned char x;
  SMS_setNextTileatXY(CLEAR_COL0, TEXT_ROW + row);
  for (x = CLEAR_COL0; x <= CLEAR_COL1; x++) SMS_setTile(TILE_FONT);
}

static void start_message (void) {
  msg_ptr = messages[msg_index];
  cur_x = TEXT_COL;
  cur_y = 0;
  state = ST_TYPING;
}

static void update_sprite (void) {
  sprite_x += sprite_dir;
  if (sprite_x >= 224) sprite_dir = -1;
  if (sprite_x <= 16)  sprite_dir = 1;

  if (++anim_timer >= 8) {
    anim_timer = 0;
    anim_frame ^= 1;
  }

  SMS_initSprites();
  SMS_addTwoAdjoiningSprites(sprite_x, SPRITE_Y, TILE_SPRITE);
  SMS_addTwoAdjoiningSprites(sprite_x, SPRITE_Y + 8,
                             TILE_SPRITE + 2 + (anim_frame ? 2 : 0));
}

void main (void) {
  SMS_displayOff();
  SMS_useFirstHalfTilesforSprites(1);
  SMS_setSpriteMode(SPRITEMODE_NORMAL);

  SMS_loadBGPalette(bg_palette);
  SMS_loadSpritePalette(spr_palette);
  SMS_setBackdropColor(0);

  SMS_load1bppTiles(font_1bpp, TILE_FONT, sizeof(font_1bpp), 0, 1);
  SMS_load1bppTiles(frame_1bpp, TILE_FRAME, sizeof(frame_1bpp), 0, 2);
  SMS_loadTiles(sprite_tiles, TILE_SPRITE, sizeof(sprite_tiles));

  draw_frame();

  sprite_x = 16;
  sprite_dir = 1;
  anim_timer = 0;
  anim_frame = 0;
  msg_index = 0;
  start_message();

  SMS_initSprites();
  SMS_displayOn();

  for (;;) {
    update_sprite();

    SMS_waitForVBlank();
    SMS_copySpritestoSAT();

    /* all VRAM writes below happen right after VBlank starts */
    switch (state) {
      case ST_TYPING: {
        char c = *msg_ptr;
        while (c == '\n') {
          cur_x = TEXT_COL;
          cur_y++;
          msg_ptr++;
          c = *msg_ptr;
        }
        if (c == 0 || cur_y >= TEXT_LINES) {
          state = ST_HOLD;
          hold_timer = HOLD_FRAMES;
        } else {
          SMS_setTileatXY(cur_x, TEXT_ROW + cur_y, char_to_tile(c));
          cur_x++;
          msg_ptr++;
        }
        break;
      }
      case ST_HOLD:
        if (--hold_timer == 0) {
          clear_row = 0;
          state = ST_CLEAR;
        }
        break;
      default: /* ST_CLEAR: one row per frame */
        clear_box_row(clear_row);
        if (++clear_row >= TEXT_LINES) {
          msg_index++;
          if (msg_index >= NUM_MESSAGES) msg_index = 0;
          start_message();
        }
        break;
    }
  }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
