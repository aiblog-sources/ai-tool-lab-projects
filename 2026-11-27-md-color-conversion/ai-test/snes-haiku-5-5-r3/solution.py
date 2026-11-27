"""SNES BGR555 to Sega Mega Drive (Genesis) CRAM color conversion."""


def _scale_5_to_3(value: int) -> int:
    """Map a 5-bit channel (0..31) to the nearest 3-bit level (0..7)."""
    # Nearest integer to value * 7 / 31, computed exactly with integers.
    return (2 * 7 * value + 31) // 62


def snes_to_md(color: int) -> int:
    """Convert a SNES 15-bit BGR555 color word to a Mega Drive CRAM word.

    SNES layout:  0BBBBBGGGGGRRRRR (bits 10-14 blue, 5-9 green, 0-4 red)
    MD CRAM:      0000BBB0GGG0RRR0 (bits 9-11 blue, 5-7 green, 1-3 red)

    Each 5-bit channel is reduced to the nearest of the 8 levels the
    Mega Drive supports per channel.

    Args:
        color: SNES color word, 0x0000..0x7FFF.

    Returns:
        Mega Drive CRAM color word, 0x0000..0x0EEE.

    Raises:
        ValueError: if color is outside the 15-bit range.
    """
    if not isinstance(color, int) or isinstance(color, bool):
        raise TypeError("color must be an int")
    if not 0 <= color <= 0x7FFF:
        raise ValueError(f"SNES color out of 15-bit range: {color!r}")

    red = _scale_5_to_3(color & 0x1F)
    green = _scale_5_to_3((color >> 5) & 0x1F)
    blue = _scale_5_to_3((color >> 10) & 0x1F)

    return (blue << 9) | (green << 5) | (red << 1)
