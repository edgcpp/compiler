#include <stdint.h>
    #include <stdio.h>
    int main(void) {
      int8_t a = -120; uint8_t b = 250;
      int16_t c = -30000; uint16_t d = 60000;
      int32_t e = -2000000000; uint32_t f = 4000000000U;
      int64_t g = -9000000000000000000LL;
      uint64_t h = 18000000000000000000ULL;
      printf("%d %u %d %u %d %u %lld %llu\n", a, b, c, d, e, f,
             (long long)(g / 7), (unsigned long long)(h / 13));
      return 0;
    }
