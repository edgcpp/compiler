#include <stdint.h>
    #include <stdio.h>
    typedef union { uint64_t u64; struct { uint32_t lo, hi; } p; } U;
    typedef struct { int tag; U payload; } Packet;
    __attribute__((noinline)) Packet flip(Packet in) {
      uint32_t t = in.payload.p.lo;
      in.payload.p.lo = in.payload.p.hi;
      in.payload.p.hi = t;
      in.tag += 1;
      return in;
    }
    int main(void) {
      Packet pk; pk.tag = 4; pk.payload.p.lo = 0x11223344U;
      pk.payload.p.hi = 0x55667788U;
      Packet q = flip(pk);
      printf("%d %x %x\n", q.tag, q.payload.p.lo, q.payload.p.hi);
      return 0;
    }
