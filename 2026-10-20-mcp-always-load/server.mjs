#!/usr/bin/env node
// notes-search: a stdio MCP server that searches a folder of Markdown files (copy of 2026-10-15, plus alwaysLoad).
// Usage: node server.mjs <folder>
// Never write to stdout here: stdout carries the JSON-RPC stream. Log to stderr.
import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import { readdirSync, readFileSync, statSync } from "node:fs";
import { join, relative, resolve } from "node:path";
import { z } from "zod";

const root = resolve(process.argv[2] ?? ".");
const tokenize = (s) => s.toLowerCase().match(/[a-z0-9][a-z0-9.\-]*[a-z0-9]|[a-z0-9]/g) ?? [];

// Load every .md file once at startup and keep a small inverted index in memory.
function walk(dir) {
  return readdirSync(dir).flatMap((name) => {
    const p = join(dir, name);
    if (name.startsWith(".") || name === "node_modules") return [];
    if (statSync(p).isDirectory()) return walk(p);
    return name.endsWith(".md") ? [p] : [];
  });
}

const docs = walk(root).map((file) => {
  const text = readFileSync(file, "utf8");
  const title = (text.match(/^title:\s*["']?(.+?)["']?\s*$/m) ?? text.match(/^#\s+(.+)$/m))?.[1] ?? file;
  const terms = tokenize(text);
  const tf = new Map();
  for (const t of terms) tf.set(t, (tf.get(t) ?? 0) + 1);
  return { path: relative(root, file), title, text, tf, len: terms.length };
});
const avgLen = docs.reduce((a, d) => a + d.len, 0) / Math.max(docs.length, 1);
const df = (t) => docs.filter((d) => d.tf.has(t)).length;

// BM25 over whole files, with a boost when a query word appears in the title.
function search(query, limit) {
  const q = [...new Set(tokenize(query))];
  return docs
    .map((d) => {
      let score = 0;
      for (const t of q) {
        const f = d.tf.get(t) ?? 0;
        if (!f) continue;
        const idf = Math.log(1 + (docs.length - df(t) + 0.5) / (df(t) + 0.5));
        score += idf * ((f * 2.2) / (f + 1.2 * (0.25 + 0.75 * (d.len / avgLen))));
        if (tokenize(d.title).includes(t)) score += idf;
      }
      return { d, score };
    })
    .filter((r) => r.score > 0)
    .sort((a, b) => b.score - a.score)
    .slice(0, limit)
    .map(({ d, score }) => ({ path: d.path, title: d.title, score: +score.toFixed(2), snippet: snippet(d.text, q) }));
}

// The paragraph that contains the most query words, cut to 400 characters.
function snippet(text, q) {
  const paras = text.split(/\n\s*\n/).filter((p) => p.trim() && !p.startsWith("---"));
  const hits = (p) => q.filter((t) => p.toLowerCase().includes(t)).length;
  const best = paras.reduce((a, b) => (hits(b) > hits(a) ? b : a), paras[0] ?? "");
  return best.replace(/\s+/g, " ").slice(0, 400);
}

// The env overrides exist only for the experiments in bench/. Defaults are the real server.
// ALWAYS_LOAD=search marks search_notes with _meta "anthropic/alwaysLoad" (Claude Code loads it upfront, not
// behind tool search). STARTUP_DELAY_MS holds the stdio connection back to imitate a slow server.
const env = process.env;
const alwaysLoad = env.ALWAYS_LOAD === "search" ? { _meta: { "anthropic/alwaysLoad": true } } : {};
const server = new McpServer({ name: "notes-search", version: "1.0.0" }, { instructions: env.NOTES_INSTRUCTIONS });

server.registerTool(
  env.SEARCH_NAME ?? "search_notes",
  {
    description: env.SEARCH_DESC ??
      `Full-text search over ${docs.length} Markdown notes and blog posts in a local folder. ` +
      "Returns the best matching files with a title, a relevance score and a snippet. " +
      "Use it before answering questions about what these notes say.",
    inputSchema: {
      query: z.string().describe("Words to look for, for example 'llama.cpp context size'"),
      limit: z.number().int().min(1).max(10).default(3).describe("How many files to return"),
    },
    ...alwaysLoad,
  },
  async ({ query, limit }) => ({ content: [{ type: "text", text: JSON.stringify(search(query, limit), null, 2) }] }),
);

server.registerTool(
  env.READ_NAME ?? "read_note",
  {
    description: env.READ_DESC ?? "Read one note in full, by the path that search_notes returned.",
    inputSchema: { path: z.string().describe("Relative path from search_notes") },
  },
  async ({ path }) => {
    const d = docs.find((x) => x.path === path);
    if (!d) return { isError: true, content: [{ type: "text", text: `No note at ${path}. Run search_notes first.` }] };
    return { content: [{ type: "text", text: d.text }] };
  },
);

if (env.STARTUP_DELAY_MS) await new Promise((r) => setTimeout(r, Number(env.STARTUP_DELAY_MS)));
await server.connect(new StdioServerTransport());
console.error(`notes-search: ${docs.length} notes indexed`);
