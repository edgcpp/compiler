#include <stddef.h>
    #include <stdint.h>
    #include <stdio.h>
    struct __attribute__((packed)) PackedRec { char c; uint64_t q; uint16_t s; };
    struct __attribute__((aligned(64))) AlignedBuf { int x; };
    int main(void) {
      struct PackedRec r = {'A', 0x0102030405060708ULL, 0x1234U};
      struct AlignedBuf b = {42};
      printf("%zu %zu %zu %d %llx\n", sizeof(r), _Alignof(struct PackedRec),
             _Alignof(struct AlignedBuf), b.x + r.c,
             (unsigned long long)r.q + r.s);
      return 0;
    }
