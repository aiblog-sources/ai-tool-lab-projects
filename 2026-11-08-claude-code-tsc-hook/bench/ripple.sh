#!/usr/bin/env bash
# How far each task's core change ripples: copy the fixture, make only the change to the core file, count tsc errors
# and the files they are in. Usage: bench/ripple.sh <scratch-folder>
set -u
root="$(cd "$(dirname "$0")/.." && pwd)"; scratch="$1"; mkdir -p "$scratch"
one() {  # <task> <file> <perl substitution>
  local w="$scratch/ripple-$1"; rm -rf "$w"; cp -a "$root/fixture" "$w"; rm -rf "$w/dist"
  perl -0pi -e "$3" "$w/$2"
  local out; out="$(cd "$w" && ./node_modules/.bin/tsc --noEmit --pretty false)"
  printf '%-15s %3d errors in %2d files\n' "$1" "$(grep -c 'error TS' <<<"$out")" "$(grep 'error TS' <<<"$out" | cut -d'(' -f1 | sort -u | wc -l)"
}
one refunded       src/types.ts 's/"cancelled";/"cancelled" | "refunded";/'
one cents          src/types.ts 's/unitPrice: number;/unitPriceCents: number;/'
one find-undefined src/store.ts 's/findOrder\(id: string\): Order \{\n    const order = this.orders.get\(id\);\n    if \(!order\) \{\n      throw new Error\(`Order \$\{id\} not found`\);\n    \}\n    return order;/findOrder(id: string): Order | undefined {\n    return this.orders.get(id);/'
one async-store    src/store.ts 's/^  (add|findOrder|list|updateStatus)\((.*)\): (\w+(?:\[\])?) \{/  async $1($2): Promise<$3> {/mg'
one currency       src/types.ts 's/  note\?: string;/  note?: string;\n  currency: "USD" | "EUR" | "BRL";/'
one name-split     src/types.ts 's/  name: string;/  firstName: string;\n  lastName: string;/'
