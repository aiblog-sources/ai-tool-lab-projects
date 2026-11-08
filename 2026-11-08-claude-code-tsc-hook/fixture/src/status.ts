import type { OrderStatus } from "./types.js";
import { assertNever } from "./assert.js";

export function statusLabel(status: OrderStatus): string {
  switch (status) {
    case "pending":
      return "Waiting for payment";
    case "paid":
      return "Paid";
    case "shipped":
      return "Shipped";
    case "cancelled":
      return "Cancelled";
    default:
      return assertNever(status);
  }
}

export function isClosed(status: OrderStatus): boolean {
  switch (status) {
    case "pending":
    case "paid":
      return false;
    case "shipped":
    case "cancelled":
      return true;
    default:
      return assertNever(status);
  }
}

export function nextStatuses(status: OrderStatus): OrderStatus[] {
  switch (status) {
    case "pending":
      return ["paid", "cancelled"];
    case "paid":
      return ["shipped", "cancelled"];
    case "shipped":
    case "cancelled":
      return [];
    default:
      return assertNever(status);
  }
}

export function canTransition(from: OrderStatus, to: OrderStatus): boolean {
  return nextStatuses(from).includes(to);
}
