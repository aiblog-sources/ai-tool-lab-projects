```python
"""Convert SNES BGR555 colors to Sega Mega Drive (Genesis) CRAM words.

SNES CGRAM color (15-bit, bit 15 ignored)::

    0BBBBBGG GGGRRRRR     R = bits 0-4, G = bits 5-9, B = bits 10-14

Mega Drive CRAM color (9-bit, stored in a 16-bit word)::

    0000BBB0 GGG0RRR0     R = bits 1-3, G = bits 5-7, B = bits 9-11

Each channel goes from 5 bits (0-31) to 3 bits (0-7). Both scales run
from black to full intensity, so the closest MD level is the nearest
point on a linear 0-7 scale: round(c * 7 / 31). The rounding uses
integer math, and a tie can't happen because 31 is odd.

Note: the real Mega Drive DAC output is not perfectly linear. Measured
levels in normal (non-shadow/highlight) mode are roughly 0, 52, 87,
116, 144, 172, 206, 255. If you want perceptual accuracy on real
hardware, use snes_to_md_hw(), which matches against those levels.
"""

__all__ = ["snes_to_md", "snes_to_md_hw", "MD_HW_LEVELS"]

# Approximate measured Mega Drive DAC output (0-255) for levels 0-7.
MD_HW_LEVELS = (0, 52, 87, 116, 144, 172, 206, 255)


def _check(color: int) -> int:
    if not isinstance(color, int) or isinstance(color, bool):
        raise TypeError(f"color must be an int, not {type(color).__name__}")
    if not 0 <= color <= 0xFFFF:
        raise ValueError(f"color out of range for a 16-bit word: {color:#x}")
    return color & 0x7FFF  # the SNES ignores bit 15


def _split(color: int) -> tuple[int, int, int]:
    return color & 0x1F, (color >> 5) & 0x1F, (color >> 10) & 0x1F


def _pack(r3: int, g3: int, b3: int) -> int:
    return (b3 << 9) | (g3 << 5) | (r3 << 1)


def _to3_linear(c5: int) -> int:
    # round(c5 * 7 / 31) using integer math
    return (c5 * 14 + 31) // 62


def snes_to_md(color: int) -> int:
    """Convert a SNES BGR555 color to the closest Mega Drive CRAM word.

    The channels are quantized on a linear scale. This is the
    conventional mapping and gives exact round trips at the endpoints:
    0x0000 -> 0x000 and 0x7FFF -> 0xEEE.

    >>> hex(snes_to_md(0x7FFF))
    '0xeee'
    >>> hex(snes_to_md(0x001F))  # pure red
    '0xe'
    >>> hex(snes_to_md(0x7C00))  # pure blue
    '0xe00'
    """
    r, g, b = _split(_check(color))
    return _pack(_to3_linear(r), _to3_linear(g), _to3_linear(b))


def _to3_hw(c5: int) -> int:
    target = c5 * 255 / 31
    return min(range(8), key=lambda i: abs(MD_HW_LEVELS[i] - target))


def snes_to_md_hw(color: int) -> int:
    """Like snes_to_md(), but matches against measured MD DAC levels.

    This gives output that looks closer on real hardware and on accurate
    emulators.
    """
    r, g, b = _split(_check(color))
    return _pack(_to3_hw(r), _to3_hw(g), _to3_hw(b))
```