#include <stdint.h>
      #include <stdio.h>
      typedef struct { unsigned char b[24]; } S;
      __attribute__((noinline)) S transform(int seed, S in, S in2) {
        S out;
        for (int i = 0; i < 24; ++i) {
          out.b[i] = (unsigned char)(in.b[i] + in2.b[i] * 3 + seed + i);
        }
        return out;
      }
      int main(void) {
        S a, b;
        for (int i = 0; i < 24; ++i) {
          a.b[i] = (unsigned char)(i + 1);
          b.b[i] = (unsigned char)(2 * i + 3);
        }
        S r = transform(11, a, b);
        unsigned sum = 0;
        for (int i = 0; i < 24; ++i) sum = sum * 31U + r.b[i];
        printf("%u\n", sum);
        return 0;
      }
