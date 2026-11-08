// Hidden acceptance tests. They import the build in <work>/dist, which the grader compiles even when tsc reports errors.
export const lib = await import(process.env.SHOP_DIST + "/index.js");
export function order(over = {}) {
  return { id: "t1", customerId: "c1", buyerId: "c1", status: "paid", createdAt: new Date("2026-02-01T00:00:00Z"),
           items: [{ sku: "BOOK-1", quantity: 2, unitPrice: 12.5, unitPriceCents: 1250 }], currency: "USD", ...over };
}
export const gold = { id: "c1", name: "Ada Lovelace", firstName: "Ada", lastName: "Lovelace", email: "ada@example.com", tier: "gold" };
