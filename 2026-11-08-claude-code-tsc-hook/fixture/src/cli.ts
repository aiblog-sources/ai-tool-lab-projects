import { OrderStore } from "./store.js";
import { customers, orders } from "./seed.js";
import { formatOrder } from "./format.js";
import { countByStatus, revenueByCustomer } from "./reports.js";
import { shippingCost } from "./rules/shipping.js";

export function main(args: string[]): string {
  const store = new OrderStore();
  for (const order of orders) store.add(order);

  const [command, id] = args;
  if (command === "show" && id) {
    const order = store.findOrder(id);
    const customer = customers.find((c) => c.id === order.customerId);
    if (!customer) return `No customer for order ${id}`;
    return `${formatOrder(order, customer)}\n  Shipping: ${shippingCost(order).toFixed(2)}`;
  }
  if (command === "pay" && id) {
    const order = store.updateStatus(id, "paid");
    return `Order ${order.id} is now ${order.status}`;
  }
  if (command === "report") {
    const counts = countByStatus(store.list());
    const revenue = revenueByCustomer(store.list(), customers);
    return JSON.stringify({ counts, revenue: Object.fromEntries(revenue) }, null, 2);
  }
  return "usage: cli show <id> | pay <id> | report";
}

if (import.meta.url === `file://${process.argv[1]}`) {
  console.log(main(process.argv.slice(2)));
}
