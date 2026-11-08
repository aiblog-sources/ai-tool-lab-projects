import type { OrderStore } from "../store.js";

export function autoCancel(store: OrderStore, ids: string[]): string[] {
  const orders = ids.map((id) => store.findOrder(id));
  const due = orders.filter((order) => order.status === "pending");
  const total = due.reduce((n, order) => n + order.items.length, 0);
  return [...due.map((order) => order.id), `autoCancel: ${due.length} orders, ${total} lines`];
}

export function autoCancelCount(store: OrderStore): number {
  return store.list((order) => order.status === "pending").length;
}
