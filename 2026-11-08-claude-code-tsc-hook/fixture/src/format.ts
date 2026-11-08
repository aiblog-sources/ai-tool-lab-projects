import type { Customer, Order } from "./types.js";
import { orderTotal } from "./pricing.js";
import { statusLabel } from "./status.js";

export function formatMoney(amount: number): string {
  return `$${amount.toFixed(2)}`;
}

export function formatOrder(order: Order, customer: Customer): string {
  const lines = order.items.map(
    (item) => `  ${item.quantity} x ${item.sku} @ ${formatMoney(item.unitPrice)}`,
  );
  return [
    `Order ${order.id} for ${customer.name} (${statusLabel(order.status)})`,
    ...lines,
    `  Total: ${formatMoney(orderTotal(order, customer))}`,
  ].join("\n");
}
