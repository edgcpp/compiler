#include <math.h>
    #include <stdint.h>
    #include <stdio.h>
    int main(void) {
      float f = 123.75f; double db = -987654.125; long double ld = 42.5L;
      int64_t i1 = (int64_t)db; uint64_t u1 = (uint64_t)(f * 1000.0f);
      printf("%lld %llu %.3f %.3f\n", (long long)i1,
             (unsigned long long)u1, (double)(f + (float)ld), db * 2.0);
      return 0;
    }
