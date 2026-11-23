#include <stdint.h>
    #include <stdio.h>
    int main(void) {
      uint32_t v = 0x001A8000U;
      uint64_t w = 0x00000F0012300000ULL;
      printf("%d %d %d %d\n", __builtin_clz(v), __builtin_ctz(v),
             __builtin_clzll(w), __builtin_ctzll(w));
      return 0;
    }
