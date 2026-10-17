#!/usr/bin/env node
// The MCP side: the same server as the 2026-10-15 post, with the index moved to notes-index.mjs.
// Usage: node server.mjs <folder>. Never write to stdout here: it carries the JSON-RPC stream.
import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import { z } from "zod";
import { loadNotes } from "./notes-index.mjs";

const notes = loadNotes(process.argv[2] ?? ".");
// SEARCH_NAME, SEARCH_DESC, READ_NAME, READ_DESC exist only for the vague-name condition in bench/. Defaults are the real server.
const env = process.env;
const server = new McpServer({ name: "notes-search", version: "1.0.0" }, { instructions: env.NOTES_INSTRUCTIONS });

server.registerTool(
  env.SEARCH_NAME ?? "search_notes",
  {
    description: env.SEARCH_DESC ??
      `Full-text search over ${notes.count} Markdown notes and blog posts in a local folder. ` +
      "Returns the best matching files with a title, a relevance score and a snippet. " +
      "Use it before answering questions about what these notes say.",
    inputSchema: {
      query: z.string().describe("Words to look for, for example 'llama.cpp context size'"),
      limit: z.number().int().min(1).max(10).default(3).describe("How many files to return"),
    },
  },
  async ({ query, limit }) => ({ content: [{ type: "text", text: JSON.stringify(notes.search(query, limit), null, 2) }] }),
);

server.registerTool(
  env.READ_NAME ?? "read_note",
  {
    description: env.READ_DESC ?? "Read one note in full, by the path that search_notes returned.",
    inputSchema: { path: z.string().describe("Relative path from search_notes") },
  },
  async ({ path }) => {
    const text = notes.read(path);
    if (!text) return { isError: true, content: [{ type: "text", text: `No note at ${path}. Run search_notes first.` }] };
    return { content: [{ type: "text", text }] };
  },
);

await server.connect(new StdioServerTransport());
console.error(`notes-search: ${notes.count} notes indexed`);
