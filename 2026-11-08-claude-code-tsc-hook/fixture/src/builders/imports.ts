import type { Customer, Order } from "../types.js";

let counter = 200;

export function importsOrder(customer: Customer, over: Partial<Order> = {}): Order {
  counter += 1;
  return {
    id: `imports-${counter}`,
    customerId: customer.id,
    status: "pending",
    items: [{ sku: "SKU-3", quantity: 3, unitPrice: 19.990000000000002 }],
    createdAt: new Date(Date.UTC(2026, 0, 3)),
    ...over,
  };
}

export function importsCustomer(id: string): Customer {
  return { id, name: "Test Imports", email: `${id}@example.com`, tier: "standard" };
}
