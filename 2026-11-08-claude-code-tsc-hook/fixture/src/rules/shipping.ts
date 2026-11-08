import type { Order } from "../types.js";
import { subtotal } from "../pricing.js";

export const FREE_SHIPPING_FROM = 50;

export function shippingCost(order: Order): number {
  if (order.status === "cancelled") return 0;
  const weightUnits = order.items.reduce((n, item) => n + item.quantity, 0);
  if (subtotal(order) >= FREE_SHIPPING_FROM) return 0;
  return 4.99 + Math.max(0, weightUnits - 3) * 0.5;
}

export function canShip(order: Order): boolean {
  return order.status === "paid" && order.items.length > 0;
}
