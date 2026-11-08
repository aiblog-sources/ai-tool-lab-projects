import test from "node:test"; import assert from "node:assert/strict";
import { lib, order, gold } from "./common.mjs";
test("refunded is closed", () => assert.equal(lib.isClosed("refunded"), true));
test("refunded only from paid or shipped", () => {
  assert.ok(lib.canTransition("paid", "refunded")); assert.ok(lib.canTransition("shipped", "refunded"));
  assert.ok(!lib.canTransition("pending", "refunded")); assert.ok(!lib.canTransition("cancelled", "refunded"));
});
test("label", () => assert.ok(typeof lib.statusLabel("refunded") === "string" && lib.statusLabel("refunded").length > 0));
test("countByStatus has refunded", () => assert.equal(lib.countByStatus([order({ status: "refunded" })]).refunded, 1));
test("refunded is zero revenue", () => {
  const r = lib.revenueByCustomer([order({ status: "refunded" }), order({ id: "t2" })], [gold]);
  assert.equal(r.get("c1"), 24.3);
});
