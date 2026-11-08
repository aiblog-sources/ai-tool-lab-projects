import test from "node:test"; import assert from "node:assert/strict";
import { lib, gold } from "./common.mjs";
const o = { id: "t1", customerId: "c1", status: "paid", createdAt: new Date(), items: [{ sku: "BOOK-1", quantity: 2, unitPriceCents: 1250 }] };
test("formatOrder prints dollars", () => { const s = lib.formatOrder(o, gold); assert.match(s, /@ \$12\.50/); assert.match(s, /Total: \$24\.30/); });
test("csv total in dollars", () => assert.match(lib.ordersToCsv([o], [gold]), /,24\.30$/m));
test("cli seed still prints dollars", async () => { const s = await lib.main(["show", "o1"]); assert.match(s, /@ \$12\.50/); assert.match(s, /Total: \$24\.30/); });
