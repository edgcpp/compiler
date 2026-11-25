#include <stdint.h>
    #include <stdio.h>
    int main(void) {
      uint32_t v = 0xA5A5F00DU;
      uint64_t w = 0x123456789ABCDEF0ULL;
      printf("%d %d %d %d %d %d\n", __builtin_popcount(v),
             __builtin_parity(v), __builtin_ffs((int)v),
             __builtin_popcountll(w), __builtin_parityll(w),
             __builtin_ffsll((long long)w));
      return 0;
    }
