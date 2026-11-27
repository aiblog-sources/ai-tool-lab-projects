```python
"""Convert 24-bit RGB colors to Sega Mega Drive (Genesis) CRAM color words."""

# Mega Drive CRAM color word layout (9-bit color, 3 bits per channel):
#   bits 15-12: unused (0)
#   bits 11-9:  Blue  (0-7)
#   bits 8-5:   bit 8 unused, Green in bits 7-5  -> see below
#
# Precisely: 0000 BBB0 GGG0 RRR0
#   Red   level (0-7) is stored in bits 1-3
#   Green level (0-7) is stored in bits 5-7
#   Blue  level (0-7) is stored in bits 9-11


def rgb_to_md(r: int, g: int, b: int) -> int:
    """
    Convert a 24-bit RGB color to a Sega Mega Drive CRAM color word.

    Each 8-bit channel (0-255) is quantized to the nearest of the eight
    3-bit levels the Mega Drive supports (0-7), using linear quantization
    (level = round(channel * 7 / 255)).

    The returned word has the layout 0000 BBB0 GGG0 RRR0, so it can be
    written directly to CRAM.

    Raises:
        ValueError: if any channel is outside the range 0-255.
    """
    for name, value in (("r", r), ("g", g), ("b", b)):
        if not 0 <= value <= 255:
            raise ValueError(f"{name} must be in range 0-255, got {value}")

    # Integer rounding of channel * 7 / 255 to the nearest level.
    r_level = (r * 7 + 127) // 255
    g_level = (g * 7 + 127) // 255
    b_level = (b * 7 + 127) // 255

    return (b_level << 9) | (g_level << 5) | (r_level << 1)
```