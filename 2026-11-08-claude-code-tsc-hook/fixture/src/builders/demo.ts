import type { Customer, Order } from "../types.js";

let counter = 300;

export function demoOrder(customer: Customer, over: Partial<Order> = {}): Order {
  counter += 1;
  return {
    id: `demo-${counter}`,
    customerId: customer.id,
    status: "pending",
    items: [{ sku: "SKU-4", quantity: 4, unitPrice: 24.990000000000002 }],
    createdAt: new Date(Date.UTC(2026, 0, 4)),
    ...over,
  };
}

export function demoCustomer(id: string): Customer {
  return { id, name: "Test Demo", email: `${id}@example.com`, tier: "standard" };
}
