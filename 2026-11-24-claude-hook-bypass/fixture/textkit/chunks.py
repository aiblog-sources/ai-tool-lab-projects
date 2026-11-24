def chunk(seq, size: int) -> list:
    """Split a sequence into lists of `size` items.

    The last list holds whatever is left and may be shorter; it is never padded and never dropped.
    size < 1 raises ValueError.

    >>> chunk([1, 2, 3, 4, 5], 2)
    [[1, 2], [3, 4], [5]]
    """
    if size < 1:
        raise ValueError("size must be >= 1")
    seq = list(seq)
    return [seq[i:i + size] for i in range(0, len(seq), size)]
