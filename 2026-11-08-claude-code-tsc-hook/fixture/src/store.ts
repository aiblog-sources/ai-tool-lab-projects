import type { Order, OrderStatus } from "./types.js";
import { canTransition } from "./status.js";

export class OrderStore {
  private orders = new Map<string, Order>();

  add(order: Order): void {
    if (this.orders.has(order.id)) {
      throw new Error(`Order ${order.id} already exists`);
    }
    this.orders.set(order.id, order);
  }

  findOrder(id: string): Order {
    const order = this.orders.get(id);
    if (!order) {
      throw new Error(`Order ${id} not found`);
    }
    return order;
  }

  list(filter?: (order: Order) => boolean): Order[] {
    const all = [...this.orders.values()];
    return filter ? all.filter(filter) : all;
  }

  updateStatus(id: string, status: OrderStatus): Order {
    const order = this.findOrder(id);
    if (!canTransition(order.status, status)) {
      throw new Error(`Cannot move order ${id} from ${order.status} to ${status}`);
    }
    const updated = { ...order, status };
    this.orders.set(id, updated);
    return updated;
  }
}
