import type { Customer } from "../types.js";
import type { OrderStore } from "../store.js";
import { formatMoney } from "../format.js";
import { orderTotal } from "../pricing.js";
import { statusLabel } from "../status.js";

export interface FraudReviewDeps {
  orders: OrderStore;
  customers: Map<string, Customer>;
}

export function fraudReviewView(deps: FraudReviewDeps, orderId: string): string[] {
  const order = deps.orders.findOrder(orderId);
  const customer = deps.customers.get(order.customerId);
  if (!customer) return [`Order ${orderId}: unknown customer ${order.customerId}`];
  const { name, email, tier } = customer;
  return [
    `fraudReview #5`,
    `Order ${order.id} (${statusLabel(order.status)})`,
    `Customer: ${name} <${email}>${tier === "gold" ? " [gold]" : ""}`,
    ...order.items.map((item) => `${item.sku}: ${item.quantity} x ${formatMoney(item.unitPrice)}`),
    `Total: ${formatMoney(orderTotal(order, customer))}`,
  ];
}
