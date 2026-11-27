"""Convert 24-bit RGB colors to Sega Mega Drive (Genesis) CRAM color words.

CRAM word layout (16 bits):

    0000 BBB0 GGG0 RRR0

Each channel is a 3-bit level (0-7) stored in bits 1-3 (red), 5-7 (green),
and 9-11 (blue). Bit 0, bit 4, bit 8, and bits 12-15 are unused and read as 0.
"""


def _quantize_channel(value: int) -> int:
    """Map an 8-bit channel value (0-255) to the nearest 3-bit level (0-7).

    The Mega Drive's DAC levels are close to evenly spaced, so this uses
    rounding to the nearest of eight evenly spaced levels. The result is
    computed with integer arithmetic: round(value * 7 / 255).
    """
    return (value * 7 + 127) // 255


def rgb_to_md(r: int, g: int, b: int) -> int:
    """Return the closest Mega Drive CRAM color word for an 8-bit RGB color.

    Args:
        r: Red channel, 0-255.
        g: Green channel, 0-255.
        b: Blue channel, 0-255.

    Returns:
        A 16-bit integer in the format 0000BBB0GGG0RRR0, ready to write to CRAM.

    Raises:
        ValueError: If any channel is not an integer in the range 0-255.
    """
    for name, value in (("r", r), ("g", g), ("b", b)):
        if not isinstance(value, int) or isinstance(value, bool):
            raise ValueError(f"{name} must be an int, got {type(value).__name__}")
        if not 0 <= value <= 255:
            raise ValueError(f"{name} must be in range 0-255, got {value}")

    r3 = _quantize_channel(r)
    g3 = _quantize_channel(g)
    b3 = _quantize_channel(b)

    return (b3 << 9) | (g3 << 5) | (r3 << 1)
