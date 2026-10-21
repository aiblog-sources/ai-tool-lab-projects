import { Router } from 'express';
import axios from 'axios';
import { z } from 'zod';
import { orderTotal } from '../lib/pricing.js';

const OrderInput = z.object({
  customerId: z.string().min(1),
  items: z.array(z.object({
    sku: z.string(),
    unitPrice: z.number().nonnegative(),
    quantity: z.number().int().positive(),
  })).min(1),
});

const orders = new Map();
export const ordersRouter = Router();

ordersRouter.get('/:id', (req, res) => {
  const order = orders.get(req.params.id);
  if (!order) return res.status(404).json({ error: 'not_found' });
  res.json(order);
});

ordersRouter.post('/', async (req, res) => {
  const parsed = OrderInput.safeParse(req.body);
  if (!parsed.success) {
    return res.status(400).json({ error: 'invalid_order', issues: parsed.error.issues });
  }
  const { data: rate } = await axios.get(`${process.env.TAX_SERVICE_URL}/rate`, {
    params: { customer: parsed.data.customerId },
    timeout: 2000,
  });
  const id = String(orders.size + 1);
  const order = { id, ...parsed.data, total: orderTotal(parsed.data.items, rate.value) };
  orders.set(id, order);
  res.status(201).json(order);
});
