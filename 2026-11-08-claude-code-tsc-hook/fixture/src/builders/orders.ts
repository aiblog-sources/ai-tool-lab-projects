import type { Customer, Order } from "../types.js";

let counter = 0;

export function ordersOrder(customer: Customer, over: Partial<Order> = {}): Order {
  counter += 1;
  return {
    id: `orders-${counter}`,
    customerId: customer.id,
    status: "pending",
    items: [{ sku: "SKU-1", quantity: 1, unitPrice: 9.99 }],
    createdAt: new Date(Date.UTC(2026, 0, 1)),
    ...over,
  };
}

export function ordersCustomer(id: string): Customer {
  return { id, name: "Test Orders", email: `${id}@example.com`, tier: "standard" };
}
