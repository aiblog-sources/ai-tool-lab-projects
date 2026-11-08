import type { Customer, Order } from "../types.js";

let counter = 100;

export function cartsOrder(customer: Customer, over: Partial<Order> = {}): Order {
  counter += 1;
  return {
    id: `carts-${counter}`,
    customerId: customer.id,
    status: "pending",
    items: [{ sku: "SKU-2", quantity: 2, unitPrice: 14.99 }],
    createdAt: new Date(Date.UTC(2026, 0, 2)),
    ...over,
  };
}

export function cartsCustomer(id: string): Customer {
  return { id, name: "Test Carts", email: `${id}@example.com`, tier: "standard" };
}
