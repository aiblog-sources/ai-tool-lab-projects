---
name: search-notes
description: Full-text search over the user's local Markdown notes and blog posts. Use it before answering questions about what these notes say.
---

# Search notes

The user's notes are a folder of Markdown files. Search them with the bundled script and answer from what it returns.

1. Search (returns the best matching files with a title, a relevance score and a snippet):

   ```bash
   node "${CLAUDE_SKILL_DIR}/search.mjs" search "words to look for" 3
   ```

2. If a snippet does not hold the answer, read the whole file by the path the search returned:

   ```bash
   node "${CLAUDE_SKILL_DIR}/search.mjs" read "path/from/search.md"
   ```

Answer from the notes. If nothing matches, say so.
