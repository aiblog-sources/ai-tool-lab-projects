import type { Customer, Order } from "./types.js";

export const customers: Customer[] = [
  { id: "c1", name: "Ada Lovelace", email: "ada@example.com", tier: "gold" },
  { id: "c2", name: "Grace Hopper", email: "grace@example.com", tier: "standard" },
];

export const orders: Order[] = [
  {
    id: "o1",
    customerId: "c1",
    status: "paid",
    items: [{ sku: "BOOK-1", quantity: 2, unitPrice: 12.5 }],
    createdAt: new Date("2026-01-05T10:00:00Z"),
  },
  {
    id: "o2",
    customerId: "c2",
    status: "pending",
    items: [
      { sku: "PEN-3", quantity: 10, unitPrice: 1.2 },
      { sku: "INK-1", quantity: 1, unitPrice: 7 },
    ],
    createdAt: new Date("2026-01-06T15:30:00Z"),
  },
  {
    id: "o3",
    customerId: "c1",
    status: "shipped",
    items: [{ sku: "LAMP-2", quantity: 1, unitPrice: 64 }],
    createdAt: new Date("2026-01-07T09:15:00Z"),
    note: "Leave at the door",
  },
];
