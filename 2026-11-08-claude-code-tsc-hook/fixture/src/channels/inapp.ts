import type { Customer, Order } from "../types.js";
import { formatMoney } from "../format.js";
import { orderTotal } from "../pricing.js";
import { statusLabel } from "../status.js";

const LIMIT = 240;

export function inappMessage(order: Order, customer: Customer): string {
  const { name } = customer;
  const items = order.items.map((item) => `${item.quantity}x ${item.sku}`).join(", ");
  const text = `${name}: order ${order.id} is ${statusLabel(order.status).toLowerCase()} (${items}), ${formatMoney(orderTotal(order, customer))}`;
  return text.length > LIMIT ? text.slice(0, LIMIT - 1) + "…" : text;
}

export function inappRecipient(customer: Customer): string {
  return customer.id;
}
