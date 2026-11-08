import type { OrderStore } from "../store.js";

export function archiveShipped(store: OrderStore, ids: string[]): string[] {
  const orders = ids.map((id) => store.findOrder(id));
  const due = orders.filter((order) => order.status === "shipped");
  const total = due.reduce((n, order) => n + order.items.length, 0);
  return [...due.map((order) => order.id), `archiveShipped: ${due.length} orders, ${total} lines`];
}

export function archiveShippedCount(store: OrderStore): number {
  return store.list((order) => order.status === "shipped").length;
}
