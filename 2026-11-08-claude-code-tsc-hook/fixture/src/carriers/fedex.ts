import type { Order } from "../types.js";
import { assertNever } from "../assert.js";

export function fedexQuote(order: Order): number | null {
  switch (order.status) {
    case "pending":
    case "cancelled":
      return null;
    case "paid":
    case "shipped": {
      const units = order.items.reduce((n, item) => n + item.quantity, 0);
      const declared = order.items.reduce((sum, item) => sum + item.unitPrice * item.quantity, 0);
      const insurance = declared > 100 ? declared * 0.01 : 0;
      return Math.round((6.2 + units * 0.75 + insurance) * 100) / 100;
    }
    default:
      return assertNever(order.status);
  }
}
