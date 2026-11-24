def paginate(items, page: int, per_page: int) -> list:
    """Return one page of items.

    Pages are numbered from 1. A page past the end returns an empty list. page < 1 or per_page < 1 raises
    ValueError.

    >>> paginate(list(range(10)), 2, 3)
    [3, 4, 5]
    """
    if page < 1 or per_page < 1:
        raise ValueError("page and per_page must be >= 1")
    items = list(items)
    start = (page - 1) * per_page
    return items[start:start + per_page]
