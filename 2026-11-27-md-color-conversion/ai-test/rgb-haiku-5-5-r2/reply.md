```python
"""Convert 24-bit RGB colors to Sega Mega Drive (Genesis) CRAM color words."""


def _quantize_channel(value: int) -> int:
    """Map an 8-bit channel (0-255) to the nearest 3-bit level (0-7).

    The Mega Drive VDP gives each channel 8 levels. This treats them as
    evenly spaced (level n ~ n * 255 / 7), so rounding to the nearest level
    minimizes the per-channel error. Ties round up.
    """
    return (value * 7 + 127) // 255


def rgb_to_md(r: int, g: int, b: int) -> int:
    """Return the closest Mega Drive CRAM color word for an 8-bit RGB color.

    The result is a 16-bit word in the CRAM layout 0000 BBB0 GGG0 RRR0:
    blue in bits 11-9, green in bits 7-5, red in bits 3-1. The top four
    bits and the low bit of each channel field are always zero.

    Each channel is quantized independently. Because the palette is a
    full 8x8x8 grid, this gives the nearest palette color in RGB space.

    Raises:
        ValueError: if any channel is outside 0-255.

    Examples:
        >>> hex(rgb_to_md(0, 0, 0))
        '0x0'
        >>> hex(rgb_to_md(255, 255, 255))
        '0xeee'
        >>> hex(rgb_to_md(255, 0, 0))
        '0xe'
        >>> hex(rgb_to_md(0, 255, 0))
        '0xe0'
        >>> hex(rgb_to_md(0, 0, 255))
        '0xe00'
    """
    for name, channel in (("r", r), ("g", g), ("b", b)):
        if not 0 <= channel <= 255:
            raise ValueError(f"{name} must be in 0..255, got {channel}")

    r3 = _quantize_channel(r)
    g3 = _quantize_channel(g)
    b3 = _quantize_channel(b)

    return (b3 << 9) | (g3 << 5) | (r3 << 1)
```