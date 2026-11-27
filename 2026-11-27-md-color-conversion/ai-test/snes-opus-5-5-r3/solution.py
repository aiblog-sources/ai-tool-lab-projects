"""Convert SNES BGR555 colors to Sega Mega Drive (Genesis) CRAM color words.

SNES color word (15 bits, bit 15 unused):
    0bbbbbgg gggrrrrr     5 bits per channel, levels 0..31

Mega Drive CRAM word (9 bits used):
    0000BBB0 GGG0RRR0     3 bits per channel, levels 0..7

Each 5-bit channel maps to the nearest 3-bit level on a linear scale:
    md = round(snes * 7 / 31)
The calculation uses integers only, so there are no float rounding surprises.
"""

__all__ = ["snes_to_md", "snes_palette_to_md"]


def _channel_5_to_3(value: int) -> int:
    """Map a 5-bit channel (0..31) to the nearest 3-bit level (0..7)."""
    # Adding 15 (about half of 31) before the division rounds to the nearest level.
    return (value * 7 + 15) // 31


def snes_to_md(color: int) -> int:
    """Convert a SNES BGR555 color word to the closest Mega Drive CRAM word.

    Bit 15 of the SNES word is ignored, as on the hardware.

    >>> hex(snes_to_md(0x0000))
    '0x0'
    >>> hex(snes_to_md(0x7FFF))
    '0xeee'
    >>> hex(snes_to_md(0x001F))  # pure red
    '0xe'
    >>> hex(snes_to_md(0x03E0))  # pure green
    '0xe0'
    >>> hex(snes_to_md(0x7C00))  # pure blue
    '0xe00'
    """
    if not isinstance(color, int) or isinstance(color, bool):
        raise TypeError(f"color must be an int, not {type(color).__name__}")
    if not 0 <= color <= 0xFFFF:
        raise ValueError(f"color out of range for a 16-bit word: {color:#x}")

    r5 = color & 0x1F
    g5 = (color >> 5) & 0x1F
    b5 = (color >> 10) & 0x1F

    r3 = _channel_5_to_3(r5)
    g3 = _channel_5_to_3(g5)
    b3 = _channel_5_to_3(b5)

    return (b3 << 9) | (g3 << 5) | (r3 << 1)


def snes_palette_to_md(colors):
    """Convert an iterable of SNES color words to a list of Mega Drive CRAM words."""
    return [snes_to_md(c) for c in colors]


if __name__ == "__main__":
    import doctest

    doctest.testmod()
