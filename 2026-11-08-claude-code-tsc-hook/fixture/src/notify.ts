import type { Customer } from "./types.js";
import type { OrderStore } from "./store.js";
import { formatMoney } from "./format.js";
import { orderTotal } from "./pricing.js";
import { statusLabel } from "./status.js";

export interface Email {
  to: string;
  subject: string;
  body: string;
}

export function statusEmail(store: OrderStore, orderId: string, customer: Customer): Email {
  const order = store.findOrder(orderId);
  return {
    to: customer.email,
    subject: `Your order ${order.id}: ${statusLabel(order.status)}`,
    body: `Hi ${customer.name},\n\nYour order is now: ${statusLabel(order.status)}.\nTotal: ${formatMoney(orderTotal(order, customer))}\n`,
  };
}
