import _ from 'lodash';

export function orderTotal(items, taxRate = 0) {
  const subtotal = _.sumBy(items, (i) => i.unitPrice * i.quantity);
  return _.round(subtotal * (1 + taxRate), 2);
}
