import type { Order, OrderStatus } from "../types.js";
import { subtotal } from "../pricing.js";

// Whether tax is owed for an order in each status, and when it becomes due.
const TAXABLE: Record<OrderStatus, boolean> = {
  pending: false,
  paid: true,
  shipped: true,
  cancelled: false,
};

export const eu_frTax = {
  rate: 0.2,
  dueAt: "paid" as OrderStatus,
  taxFor(order: Order): number {
    if (!TAXABLE[order.status]) return 0;
    return Math.round(subtotal(order) * 0.2 * 100) / 100;
  },
};
