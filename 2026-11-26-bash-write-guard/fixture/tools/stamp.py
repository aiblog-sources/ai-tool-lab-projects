"""Add a header comment to every Python file in the test folder (a stand-in for any project script)."""
from pathlib import Path

for f in Path("tests").glob("*.py"):
    text = f.read_text()
    if not text.startswith("# checked\n"):
        f.write_text("# checked\n" + text)
