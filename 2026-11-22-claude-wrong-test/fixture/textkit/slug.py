import re


def slugify(text: str) -> str:
    """Turn a title into a URL slug.

    Non-ASCII characters are dropped first, not transliterated. Then the text is lowercased, every run of
    characters that are not letters or digits becomes a single hyphen, and hyphens are stripped from both
    ends. "Café au lait" becomes "caf-au-lait".

    >>> slugify("Hello, World!")
    'hello-world'
    """
    text = text.encode("ascii", "ignore").decode()
    return re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")
