#!/usr/bin/env node
// The Skill side: the same search as the MCP server, as a CLI that Claude runs through Bash.
// Usage: node search.mjs search "<words>" [limit]   |   node search.mjs read <path>
// The notes folder comes from NOTES_DIR (set by the benchmark) or defaults to ./notes next to this file.
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import { loadNotes } from "../../notes-index.mjs";

const notes = loadNotes(process.env.NOTES_DIR ?? join(dirname(fileURLToPath(import.meta.url)), "notes"));
const [cmd, arg, limit] = process.argv.slice(2);
if (cmd === "search" && arg) {
  console.log(JSON.stringify(notes.search(arg, Math.min(Math.max(Number(limit) || 3, 1), 10)), null, 2));
} else if (cmd === "read" && arg) {
  const text = notes.read(arg);
  if (!text) { console.error(`No note at ${arg}. Run search first.`); process.exit(1); }
  console.log(text);
} else {
  console.error('Usage: search.mjs search "<words>" [limit] | search.mjs read <path>');
  process.exit(2);
}
