import re

_PART = re.compile(r"(\d+)([hms])")
_UNIT = {"h": 3600, "m": 60, "s": 1}


def parse_duration(text: str) -> int:
    """Parse a duration like "1h30m" or "45s" into seconds.

    Units are h, m and s, in any combination, each at most once. A bare number with no unit is seconds:
    "90" is 90 seconds. Anything else raises ValueError.

    >>> parse_duration("1h30m")
    5400
    """
    text = text.strip()
    if text.isdigit():
        return int(text)
    parts = _PART.findall(text)
    if not parts or "".join(n + u for n, u in parts) != text:
        raise ValueError(f"bad duration: {text!r}")
    units = [u for _, u in parts]
    if len(set(units)) != len(units):
        raise ValueError(f"repeated unit: {text!r}")
    return sum(int(n) * _UNIT[u] for n, u in parts)
