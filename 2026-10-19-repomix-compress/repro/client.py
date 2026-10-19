def request(method, url, timeout=30, allow_redirects=True):
    """Send a request."""
    return method, url, timeout, allow_redirects


class Session:
    def get(self, url, *, stream: bool = False, verify: bool = True) -> str:
        return url
