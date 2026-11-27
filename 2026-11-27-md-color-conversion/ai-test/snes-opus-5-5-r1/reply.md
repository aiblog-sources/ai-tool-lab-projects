```python
"""Convert SNES BGR555 colors to Sega Mega Drive / Genesis CRAM words.

SNES CGRAM word (15-bit, bit 15 unused):
    0BBBBBGG GGGRRRRR   -> 5 bits per channel (0..31)

Mega Drive CRAM word (9-bit color):
    0000BBB0 GGG0RRR0   -> 3 bits per channel (0..7), each at bits 1-3 of its nibble

The Mega Drive's video DAC is not linear, so dividing each 5-bit channel by
4 gives visibly wrong results, especially in the darks. This module converts
each SNES channel to 8-bit intensity and picks the Mega Drive level whose
measured output intensity is closest. Channels are independent, so choosing
the nearest level per channel also minimizes the Euclidean RGB distance of
the whole color.
"""

__all__ = ["snes_to_md", "MD_LEVELS_NORMAL", "MD_LEVELS_LINEAR"]

# Measured Mega Drive DAC output (normal mode, no shadow/highlight), 0..255.
MD_LEVELS_NORMAL = (0, 52, 87, 116, 144, 172, 206, 255)

# Idealized linear ramp, for emulators or tools that assume a linear DAC.
MD_LEVELS_LINEAR = tuple(round(i * 255 / 7) for i in range(8))


def _snes_channel_to_8bit(value: int) -> int:
    """Expand a 5-bit channel to 8 bits by bit replication (0 -> 0, 31 -> 255)."""
    return (value << 3) | (value >> 2)


def _nearest_level(intensity: int, levels) -> int:
    """Return the index (0..7) of the level closest to intensity.

    On a tie, the lower index wins, which keeps results deterministic.
    """
    best_index = 0
    best_distance = abs(levels[0] - intensity)
    for index in range(1, len(levels)):
        distance = abs(levels[index] - intensity)
        if distance < best_distance:
            best_index = index
            best_distance = distance
    return best_index


def snes_to_md(color: int, *, levels=MD_LEVELS_NORMAL) -> int:
    """Convert a SNES BGR555 color word to the closest Mega Drive CRAM word.

    Args:
        color: SNES color word, 0x0000..0xFFFF. Bit 15 is ignored, as on hardware.
        levels: Eight increasing 0..255 intensities of the Mega Drive DAC.
            The default uses measured hardware levels. Pass MD_LEVELS_LINEAR
            for a plain linear mapping.

    Returns:
        The Mega Drive CRAM word (0x000..0xEEE) in 0000BBB0GGG0RRR0 format.

    Raises:
        TypeError: if color is not an int.
        ValueError: if color is outside 0..0xFFFF or levels is malformed.

    Examples:
        >>> hex(snes_to_md(0x0000))
        '0x0'
        >>> hex(snes_to_md(0x7FFF))
        '0xeee'
        >>> hex(snes_to_md(0x001F))   # pure red
        '0xe'
        >>> hex(snes_to_md(0x7C00))   # pure blue
        '0xe00'
    """
    if isinstance(color, bool) or not isinstance(color, int):
        raise TypeError(f"color must be an int, not {type(color).__name__}")
    if not 0 <= color <= 0xFFFF:
        raise ValueError(f"color out of range for a 16-bit word: {color:#x}")
    if len(levels) != 8:
        raise ValueError("levels must contain exactly 8 intensities")

    color &= 0x7FFF  # bit 15 is unused on the SNES

    red = _snes_channel_to_8bit(color & 0x1F)
    green = _snes_channel_to_8bit((color >> 5) & 0x1F)
    blue = _snes_channel_to_8bit((color >> 10) & 0x1F)

    md_red = _nearest_level(red, levels)
    md_green = _nearest_level(green, levels)
    md_blue = _nearest_level(blue, levels)

    return (md_blue << 9) | (md_green << 5) | (md_red << 1)
```