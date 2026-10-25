---
name: search-notes
description: Full-text search over the user's local Markdown notes and blog posts. Use it before answering questions about what these notes say.
---

# Search notes

The user's notes are a folder of Markdown files, served by the `notes` MCP server. Search them with its tools and
answer from what they return.

1. Search with the `mcp__notes__search_notes` tool (arguments: `query`, the words to look for, and `limit`, default 3).
   It returns the best matching files with a title, a relevance score and a snippet.

2. If a snippet does not hold the answer, read the whole file with `mcp__notes__read_note`, passing the `path` that
   the search returned.

Answer from the notes. If nothing matches, say so.
