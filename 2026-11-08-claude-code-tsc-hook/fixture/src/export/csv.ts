import type { Customer, Order } from "../types.js";
import { orderTotal } from "../pricing.js";

function escape(value: string): string {
  return /[",\n]/.test(value) ? `"${value.replace(/"/g, '""')}"` : value;
}

export function ordersToCsv(orders: Order[], customers: Customer[]): string {
  const byId = new Map(customers.map((c) => [c.id, c]));
  const rows = ["id,customer,status,items,total"];
  for (const order of orders) {
    const customer = byId.get(order.customerId);
    if (!customer) continue;
    const items = order.items.reduce((n, item) => n + item.quantity, 0);
    rows.push(
      [order.id, escape(customer.name), order.status, String(items), orderTotal(order, customer).toFixed(2)].join(","),
    );
  }
  return rows.join("\n");
}
