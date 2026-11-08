import type { Customer, LineItem, Order } from "./types.js";

export const TAX_RATE = 0.08;
const GOLD_DISCOUNT = 0.1;

export function lineTotal(item: LineItem): number {
  return item.quantity * item.unitPrice;
}

export function subtotal(order: Order): number {
  return order.items.reduce((sum, item) => sum + lineTotal(item), 0);
}

export function discount(order: Order, customer: Customer): number {
  return customer.tier === "gold" ? subtotal(order) * GOLD_DISCOUNT : 0;
}

export function orderTotal(order: Order, customer: Customer): number {
  const net = subtotal(order) - discount(order, customer);
  return Math.round(net * (1 + TAX_RATE) * 100) / 100;
}
