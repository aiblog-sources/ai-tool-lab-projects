// Spawns the server N times and times: process start to initialize reply, tools/list, one search.
// Usage: node bench/handshake.mjs <folder> [runs] [server.mjs]
import { spawn } from "node:child_process";
import { performance } from "node:perf_hooks";

const [folder, runs = "20", script = "server.mjs"] = process.argv.slice(2);

function once() {
  return new Promise((resolveRun, reject) => {
    const t0 = performance.now();
    const p = spawn(process.execPath, [script, folder], { stdio: ["pipe", "pipe", "pipe"] });
    const marks = {}, pending = new Map();
    let buf = "", stdoutJunk = 0;
    p.stdout.on("data", (chunk) => {
      buf += chunk;
      let i;
      while ((i = buf.indexOf("\n")) >= 0) {
        const line = buf.slice(0, i); buf = buf.slice(i + 1);
        let msg; try { msg = JSON.parse(line); } catch { stdoutJunk++; continue; }
        pending.get(msg.id)?.(msg);
      }
    });
    const send = (id, method, params) => new Promise((res) => {
      pending.set(id, res);
      p.stdin.write(JSON.stringify({ jsonrpc: "2.0", id, method, params }) + "\n");
    });
    (async () => {
      await send(1, "initialize", { protocolVersion: "2025-06-18", capabilities: {}, clientInfo: { name: "bench", version: "1" } });
      marks.initialize = performance.now() - t0;
      p.stdin.write(JSON.stringify({ jsonrpc: "2.0", method: "notifications/initialized" }) + "\n");
      let t = performance.now();
      const list = await send(2, "tools/list", {});
      marks.list = performance.now() - t;
      t = performance.now();
      const call = await send(3, "tools/call", { name: "search_notes", arguments: { query: "llama.cpp context size", limit: 3 } });
      marks.search = performance.now() - t;
      p.kill();
      resolveRun({ ...marks, tools: list.result.tools.map((x) => x.name), listBytes: JSON.stringify(list.result).length,
        top: JSON.parse(call.result.content[0].text).map((r) => r.path), stdoutJunk });
    })().catch(reject);
  });
}

const results = [];
for (let i = 0; i < +runs; i++) results.push(await once());
const med = (k) => { const v = results.map((r) => r[k]).sort((a, b) => a - b); return +v[Math.floor(v.length / 2)].toFixed(1); };
const max = (k) => +Math.max(...results.map((r) => r[k])).toFixed(1);
console.log(JSON.stringify({ runs: results.length, median_ms: { initialize: med("initialize"), list: med("list"), search: med("search") },
  max_ms: { initialize: max("initialize"), list: max("list"), search: max("search") },
  tools: results[0].tools, tools_list_bytes: results[0].listBytes, top: results[0].top }, null, 2));
