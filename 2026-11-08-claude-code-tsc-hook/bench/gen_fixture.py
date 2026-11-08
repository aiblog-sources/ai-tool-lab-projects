#!/usr/bin/env python3
"""Grow fixture/src from the 13 hand-written core files into a ~55-file app.

The extra modules come from six templates (notification channels, regional tax rules, carriers, admin views, batch
jobs, test-data builders), each filled in a few times with different names and numbers, so a refactor of the core
types has to reach many files that use them in different ways: destructuring, Record<OrderStatus, ...> tables,
exhaustive switches, an injected store, Promise-free batch maps and object literals. Run once; the output is committed.
"""
from pathlib import Path
src = Path(__file__).resolve().parent.parent / "fixture/src"

def w(rel, text):
    p = src / rel; p.parent.mkdir(parents=True, exist_ok=True); p.write_text(text.lstrip())

channels = {"email": 72, "sms": 160, "push": 90, "slack": 300, "webhook": 2000, "whatsapp": 1000, "telegram": 4096, "inapp": 240}
for name, limit in channels.items():
    w(f"channels/{name}.ts", f'''
import type {{ Customer, Order }} from "../types.js";
import {{ formatMoney }} from "../format.js";
import {{ orderTotal }} from "../pricing.js";
import {{ statusLabel }} from "../status.js";

const LIMIT = {limit};

export function {name}Message(order: Order, customer: Customer): string {{
  const {{ name }} = customer;
  const items = order.items.map((item) => `${{item.quantity}}x ${{item.sku}}`).join(", ");
  const text = `${{name}}: order ${{order.id}} is ${{statusLabel(order.status).toLowerCase()}} (${{items}}), ${{formatMoney(orderTotal(order, customer))}}`;
  return text.length > LIMIT ? text.slice(0, LIMIT - 1) + "…" : text;
}}

export function {name}Recipient(customer: Customer): string {{
  return customer.{"email" if name in ("email", "webhook") else "id"};
}}
''')

regions = {"us_ca": (0.0725, "shipped"), "us_ny": (0.04, "paid"), "eu_de": (0.19, "paid"), "eu_fr": (0.2, "paid"),
           "eu_pt": (0.23, "paid"), "uk": (0.2, "shipped"), "br_sp": (0.18, "paid"), "jp": (0.1, "shipped")}
for code, (rate, due) in regions.items():
    w(f"regions/{code}.ts", f'''
import type {{ Order, OrderStatus }} from "../types.js";
import {{ subtotal }} from "../pricing.js";

// Whether tax is owed for an order in each status, and when it becomes due.
const TAXABLE: Record<OrderStatus, boolean> = {{
  pending: false,
  paid: true,
  shipped: true,
  cancelled: false,
}};

export const {code}Tax = {{
  rate: {rate},
  dueAt: "{due}" as OrderStatus,
  taxFor(order: Order): number {{
    if (!TAXABLE[order.status]) return 0;
    return Math.round(subtotal(order) * {rate} * 100) / 100;
  }},
}};
''')

carriers = {"ups": (5.5, 0.8), "fedex": (6.2, 0.75), "dhl": (7.0, 0.9), "usps": (3.9, 0.5), "correios": (4.5, 0.6), "royalmail": (4.1, 0.55)}
for name, (base, per) in carriers.items():
    w(f"carriers/{name}.ts", f'''
import type {{ Order }} from "../types.js";
import {{ assertNever }} from "../assert.js";

export function {name}Quote(order: Order): number | null {{
  switch (order.status) {{
    case "pending":
    case "cancelled":
      return null;
    case "paid":
    case "shipped": {{
      const units = order.items.reduce((n, item) => n + item.quantity, 0);
      const declared = order.items.reduce((sum, item) => sum + item.unitPrice * item.quantity, 0);
      const insurance = declared > 100 ? declared * 0.01 : 0;
      return Math.round(({base} + units * {per} + insurance) * 100) / 100;
    }}
    default:
      return assertNever(order.status);
  }}
}}
''')

admin = ["orderDetail", "orderTimeline", "customerOrders", "refundDesk", "fraudReview", "packingSlip", "invoice", "supportTicket"]
for i, page in enumerate(admin):
    w(f"admin/{page}.ts", f'''
import type {{ Customer }} from "../types.js";
import type {{ OrderStore }} from "../store.js";
import {{ formatMoney }} from "../format.js";
import {{ orderTotal }} from "../pricing.js";
import {{ statusLabel }} from "../status.js";

export interface {page[0].upper() + page[1:]}Deps {{
  orders: OrderStore;
  customers: Map<string, Customer>;
}}

export function {page}View(deps: {page[0].upper() + page[1:]}Deps, orderId: string): string[] {{
  const order = deps.orders.findOrder(orderId);
  const customer = deps.customers.get(order.customerId);
  if (!customer) return [`Order ${{orderId}}: unknown customer ${{order.customerId}}`];
  const {{ name, email, tier }} = customer;
  return [
    `{page} #{i + 1}`,
    `Order ${{order.id}} (${{statusLabel(order.status)}})`,
    `Customer: ${{name}} <${{email}}>${{tier === "gold" ? " [gold]" : ""}}`,
    ...order.items.map((item) => `${{item.sku}}: ${{item.quantity}} x ${{formatMoney(item.unitPrice)}}`),
    `Total: ${{formatMoney(orderTotal(order, customer))}}`,
  ];
}}
''')

jobs = {"autoCancel": "pending", "shipPaid": "paid", "remindUnpaid": "pending", "archiveShipped": "shipped", "auditTotals": "paid", "syncWarehouse": "paid"}
for name, status in jobs.items():
    w(f"jobs/{name}.ts", f'''
import type {{ OrderStore }} from "../store.js";

export function {name}(store: OrderStore, ids: string[]): string[] {{
  const orders = ids.map((id) => store.findOrder(id));
  const due = orders.filter((order) => order.status === "{status}");
  const total = due.reduce((n, order) => n + order.items.length, 0);
  return [...due.map((order) => order.id), `{name}: ${{due.length}} orders, ${{total}} lines`];
}}

export function {name}Count(store: OrderStore): number {{
  return store.list((order) => order.status === "{status}").length;
}}
''')

builders = {"orders": "Order", "carts": "Order", "imports": "Order", "demo": "Order"}
for i, name in enumerate(builders):
    w(f"builders/{name}.ts", f'''
import type {{ Customer, Order }} from "../types.js";

let counter = {i * 100};

export function {name}Order(customer: Customer, over: Partial<Order> = {{}}): Order {{
  counter += 1;
  return {{
    id: `{name}-${{counter}}`,
    customerId: customer.id,
    status: "pending",
    items: [{{ sku: "SKU-{i + 1}", quantity: {i + 1}, unitPrice: {9.99 + i * 5} }}],
    createdAt: new Date(Date.UTC(2026, 0, {i + 1})),
    ...over,
  }};
}}

export function {name}Customer(id: string): Customer {{
  return {{ id, name: "Test {name.capitalize()}", email: `${{id}}@example.com`, tier: "standard" }};
}}
''')
print(sum(1 for _ in src.rglob("*.ts")), "files")
