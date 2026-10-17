// Shared by the MCP server and the Skill's CLI, so both paths return byte-identical results.
import { readdirSync, readFileSync, statSync } from "node:fs";
import { join, relative, resolve } from "node:path";

const tokenize = (s) => s.toLowerCase().match(/[a-z0-9][a-z0-9.\-]*[a-z0-9]|[a-z0-9]/g) ?? [];

function walk(dir) {
  return readdirSync(dir).flatMap((name) => {
    const p = join(dir, name);
    if (name.startsWith(".") || name === "node_modules") return [];
    if (statSync(p).isDirectory()) return walk(p);
    return name.endsWith(".md") ? [p] : [];
  });
}

export function loadNotes(folder) {
  const root = resolve(folder);
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
  function search(query, limit = 3) {
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

  const read = (path) => docs.find((x) => x.path === path)?.text;
  return { count: docs.length, search, read };
}

// The paragraph that contains the most query words, cut to 400 characters.
function snippet(text, q) {
  const paras = text.split(/\n\s*\n/).filter((p) => p.trim() && !p.startsWith("---"));
  const hits = (p) => q.filter((t) => p.toLowerCase().includes(t)).length;
  const best = paras.reduce((a, b) => (hits(b) > hits(a) ? b : a), paras[0] ?? "");
  return best.replace(/\s+/g, " ").slice(0, 400);
}
