#include <stdint.h>
    #include <stdio.h>
    struct BF {
      signed int a:5;
      unsigned int b:7;
      signed int c:11;
      unsigned int :0;
      signed int d:9;
      unsigned long long e:37;
    };
    int main(void) {
      struct BF x = {-13, 109, -900, -211, 0x1ABCDEF123ULL};
      x.a += 4; x.b ^= 0x3AU; x.c -= 23; x.d += 17; x.e += 99ULL;
      printf("%d %u %d %d %llu\n", x.a, x.b, x.c, x.d,
             (unsigned long long)x.e);
      return 0;
    }
