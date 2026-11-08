import test from "node:test"; import assert from "node:assert/strict";
import { lib, order } from "./common.mjs";
const c = { id: "c1", firstName: "Ada", lastName: "Lovelace", email: "ada@example.com", tier: "gold" };
test("formatOrder full name", () => assert.match(lib.formatOrder(order(), c), /for Ada Lovelace/));
test("csv full name", () => assert.match(lib.ordersToCsv([order()], [c]), /,Ada Lovelace,/));
test("email greets by first name", () => {
  const s = new lib.OrderStore(); s.add(order());
  const e = lib.statusEmail(s, "t1", c); assert.match(e.body, /^Hi Ada,/); assert.doesNotMatch(e.body, /Lovelace/);
});
test("cli seed full name", () => assert.match(lib.main(["show", "o1"]), /for Ada Lovelace/));
