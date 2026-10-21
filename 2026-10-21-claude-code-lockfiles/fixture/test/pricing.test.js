import { describe, expect, it } from 'vitest';
import { orderTotal } from '../src/lib/pricing.js';

describe('orderTotal', () => {
  it('adds tax and rounds to cents', () => {
    expect(orderTotal([{ unitPrice: 9.99, quantity: 3 }], 0.2)).toBe(35.96);
  });
});
