def snes_to_md(color: int) -> int:
    """
    Convert a SNES BGR555 color word to the nearest Sega Mega Drive CRAM word.

    SNES layout (15-bit):  0BBBBBGGGGGRRRRR  (5 bits per channel)
    MD CRAM layout (9-bit): 0000BBB0GGG0RRR0 (3 bits per channel, stored at bits 1-3, 5-7, 9-11)

    Each 5-bit channel is mapped to the nearest of the 8 MD levels (0..7)
    by linear rounding: level = round(v * 7 / 31).

    Raises ValueError if color is not in the range 0..0x7FFF.
    """
    if not isinstance(color, int) or not 0 <= color <= 0x7FFF:
        raise ValueError("SNES color must be an integer in the range 0..0x7FFF")

    red = color & 0x1F
    green = (color >> 5) & 0x1F
    blue = (color >> 10) & 0x1F

    def reduce_channel(v: int) -> int:
        # Round-half-up of v * 7 / 31, done in integer arithmetic.
        return (v * 7 + 15) // 31

    r = reduce_channel(red)
    g = reduce_channel(green)
    b = reduce_channel(blue)

    return (b << 9) | (g << 5) | (r << 1)
