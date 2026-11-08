import type { Order, OrderStatus } from "../types.js";
import { subtotal } from "../pricing.js";

// Whether tax is owed for an order in each status, and when it becomes due.
const TAXABLE: Record<OrderStatus, boolean> = {
  pending: false,
  paid: true,
  shipped: true,
  cancelled: false,
};

export const us_caTax = {
  rate: 0.0725,
  dueAt: "shipped" as OrderStatus,
  taxFor(order: Order): number {
    if (!TAXABLE[order.status]) return 0;
    return Math.round(subtotal(order) * 0.0725 * 100) / 100;
  },
};
