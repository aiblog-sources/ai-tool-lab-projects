import type { Customer, Order, OrderStatus } from "./types.js";
import { orderTotal } from "./pricing.js";
import { isClosed } from "./status.js";

export function countByStatus(orders: Order[]): Record<OrderStatus, number> {
  const counts: Record<OrderStatus, number> = {
    pending: 0,
    paid: 0,
    shipped: 0,
    cancelled: 0,
  };
  for (const order of orders) {
    counts[order.status] += 1;
  }
  return counts;
}

export function revenueByCustomer(orders: Order[], customers: Customer[]): Map<string, number> {
  const byId = new Map(customers.map((c) => [c.id, c]));
  const revenue = new Map<string, number>();
  for (const order of orders) {
    if (order.status === "cancelled" || order.status === "pending") continue;
    const customer = byId.get(order.customerId);
    if (!customer) continue;
    revenue.set(customer.id, (revenue.get(customer.id) ?? 0) + orderTotal(order, customer));
  }
  return revenue;
}

export function openOrders(orders: Order[]): Order[] {
  return orders.filter((order) => !isClosed(order.status));
}
