#include "random.h"
#include "types.h"
static ulong rng_state = 88172645463325252ULL;

static inline ulong next() {
  ulong x = rng_state;
  x ^= x << 13;
  x ^= x >> 7;
  x ^= x << 17;
  rng_state = x;
  return x;
}

ulong rand64() { return next(); }
uint rand32() { return (uint)next(); }
ushort rand16() { return (ushort)next(); }
ubyte rand8() { return (ubyte)next(); }
ulong rand_range(ulong min, ulong max) {
  return min + (next() % (max - min));
}