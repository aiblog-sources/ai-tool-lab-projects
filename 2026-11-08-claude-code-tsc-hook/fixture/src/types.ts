export type OrderStatus = "pending" | "paid" | "shipped" | "cancelled";

export interface LineItem {
  sku: string;
  quantity: number;
  unitPrice: number;
}

export interface Order {
  id: string;
  customerId: string;
  status: OrderStatus;
  items: LineItem[];
  createdAt: Date;
  note?: string;
}

export interface Customer {
  id: string;
  name: string;
  email: string;
  tier: "standard" | "gold";
}
