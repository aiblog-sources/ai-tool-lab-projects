# cc65 issue #288 on cc65 eebf54a (2026-11-08)

`issue288.c` is the example from https://github.com/cc65/cc65/issues/288 (8-bit index into a constant address),
with a static index as in the issue, plus `char` and `unsigned char` locals. `issue288-eebf54a-Oirs.s` is
`cc65 -t nes -Oirs issue288.c` from the commit this project uses.

With a static index the loop no longer calls `pushax` or `staspidx`: the read is `lda $6000,y`, but the destination
is still rebuilt in `ptr1` on every byte (`sta (ptr1),y`). With a local index it goes through the C stack.
