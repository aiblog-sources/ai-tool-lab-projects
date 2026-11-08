import test from "node:test"; import assert from "node:assert/strict";
import { lib, order, gold } from "./common.mjs";
test("methods return promises", async () => {
  const s = new lib.OrderStore();
  const a = s.add(order()); assert.ok(a instanceof Promise); await a;
  const f = s.findOrder("t1"); assert.ok(f instanceof Promise); assert.equal((await f).id, "t1");
  const l = s.list(); assert.ok(l instanceof Promise); assert.equal((await l).length, 1);
  const u = s.updateStatus("t1", "shipped"); assert.ok(u instanceof Promise); assert.equal((await u).status, "shipped");
});
test("unknown id rejects", async () => { const s = new lib.OrderStore(); await assert.rejects(async () => s.findOrder("nope")); });
test("cli show", async () => assert.match(await lib.main(["show", "o1"]), /Order o1 for Ada Lovelace/));
test("cli report", async () => assert.match(await lib.main(["report"]), /"pending": 1/));
test("statusEmail", async () => {
  const s = new lib.OrderStore(); await s.add(order());
  const e = await lib.statusEmail(s, "t1", gold); assert.match(e.subject, /t1/); assert.match(e.body, /\$24\.30/);
});
