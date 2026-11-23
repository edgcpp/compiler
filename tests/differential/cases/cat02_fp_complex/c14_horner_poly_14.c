#include <math.h>
      #include <stdio.h>
      int main(void) {
        double x = 5.2500, c = -6.7500, acc = 0.0;
        for (int i = 1; i <= 16; ++i) {
          acc = acc * 0.5 + (x * (double)i - c) / (double)(i + 1);
        }
        printf("%.6f\n", acc);
        return 0;
      }
