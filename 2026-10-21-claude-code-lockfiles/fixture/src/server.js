import 'dotenv/config';
import express from 'express';
import { logger } from './lib/logger.js';
import { requireAuth } from './lib/auth.js';
import { ordersRouter } from './routes/orders.js';

const app = express();
app.use(express.json());

app.use('/orders', requireAuth, ordersRouter);

app.use((err, req, res, next) => {
  logger.error({ err }, 'unhandled error');
  res.status(500).json({ error: 'internal_error' });
});

const port = Number(process.env.PORT ?? 3000);
app.listen(port, () => logger.info({ port }, 'orders-api listening'));

export { app };
