```python
"""Convert SNES BGR555 colors to Sega Mega Drive (Genesis) CRAM words.

SNES color word (15 bits):  0 BBBBB GGGGG RRRRR
Mega Drive CRAM word (9 bits used): 0000 BBB0 GGG0 RRR0

Each 5-bit SNES channel is reduced to the nearest of the eight 3-bit
Mega Drive levels. Level k has intensity k/7 and SNES level v has
intensity v/31, so the nearest k minimizes |7*v - 31*k|. Because 31 is
odd, 7*v/31 is never exactly halfway between two integers, so there are
no ties and rounding is unambiguous.
"""


def _scale_5_to_3(v: int) -> int:
    """Return the 3-bit level closest to 5-bit level v (0..31)."""
    return (7 * v + 15) // 31


def snes_to_md(color: int) -> int:
    """Convert a SNES 15-bit BGR555 color to a Mega Drive CRAM color word.

    Args:
        color: SNES color word, 0x0000..0x7FFF. Bit 15 must be clear.

    Returns:
        Mega Drive CRAM word in the form 0000 BBB0 GGG0 RRR0 (0x0000..0x0EEE).

    Raises:
        ValueError: if color is outside 0..0x7FFF.
    """
    if not isinstance(color, int) or not 0 <= color <= 0x7FFF:
        raise ValueError(f"SNES color must be in 0..0x7FFF, got {color!r}")

    r5 = color & 0x1F
    g5 = (color >> 5) & 0x1F
    b5 = (color >> 10) & 0x1F

    r3 = _scale_5_to_3(r5)
    g3 = _scale_5_to_3(g5)
    b3 = _scale_5_to_3(b5)

    return (b3 << 9) | (g3 << 5) | (r3 << 1)
```