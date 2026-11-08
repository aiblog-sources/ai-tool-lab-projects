import test from "node:test"; import assert from "node:assert/strict";
import { lib, order } from "./common.mjs";
test("findOrder unknown is undefined", () => { const s = new lib.OrderStore(); assert.equal(s.findOrder("nope"), undefined); });
test("findOrder known", () => { const s = new lib.OrderStore(); s.add(order()); assert.equal(s.findOrder("t1").id, "t1"); });
test("updateStatus unknown throws", () => { const s = new lib.OrderStore(); assert.throws(() => s.updateStatus("nope", "paid")); });
test("cli unknown id does not crash", () => { const out = lib.main(["show", "nope"]); assert.equal(typeof out, "string"); assert.ok(out.length > 0); });
test("cli known id still works", () => assert.match(lib.main(["show", "o1"]), /Order o1/));
test("statusEmail unknown id does not return a broken email", () => {
  let r; try { r = lib.statusEmail(new lib.OrderStore(), "nope", { id: "c1", name: "A", email: "a@x", tier: "gold" }); } catch { return; }
  assert.ok(r === undefined || r === null, "should throw or return nothing");
});
