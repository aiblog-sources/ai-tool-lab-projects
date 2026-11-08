import test from "node:test"; import assert from "node:assert/strict";
import { lib, order, gold } from "./common.mjs";
test("EUR uses euro sign", () => { const s = lib.formatOrder(order({ currency: "EUR" }), gold); assert.match(s, /€/); assert.doesNotMatch(s, /\$/); assert.match(s, /24\.30/); });
test("BRL uses R$", () => { const s = lib.formatOrder(order({ currency: "BRL" }), gold); assert.match(s, /R\$/); assert.doesNotMatch(s, /€/); });
test("USD unchanged", () => assert.match(lib.formatOrder(order({ currency: "USD" }), gold), /Total: \$24\.30/));
test("seed orders are USD", async () => assert.match(await lib.main(["show", "o1"]), /Total: \$24\.30/));
