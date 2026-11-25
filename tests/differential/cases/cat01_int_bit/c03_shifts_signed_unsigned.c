#include <stdint.h>
    #include <stdio.h>
    int main(void) {
      int64_t s = -0x123456789ABCLL;
      uint64_t u = 0xFEDCBA9876543210ULL;
      uint64_t acc = 0;
      for (int k = 1; k < 63; k += 5) {
        acc ^= (uint64_t)(s >> k) + (u >> k) + (u << (64 - k));
      }
      printf("%llx\n", (unsigned long long)acc);
      return 0;
    }
