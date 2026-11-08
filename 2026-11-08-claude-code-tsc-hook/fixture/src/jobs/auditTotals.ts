import type { OrderStore } from "../store.js";

export function auditTotals(store: OrderStore, ids: string[]): string[] {
  const orders = ids.map((id) => store.findOrder(id));
  const due = orders.filter((order) => order.status === "paid");
  const total = due.reduce((n, order) => n + order.items.length, 0);
  return [...due.map((order) => order.id), `auditTotals: ${due.length} orders, ${total} lines`];
}

export function auditTotalsCount(store: OrderStore): number {
  return store.list((order) => order.status === "paid").length;
}
