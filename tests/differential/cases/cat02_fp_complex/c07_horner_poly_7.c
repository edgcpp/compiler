#include <math.h>
      #include <stdio.h>
      int main(void) {
        double x = 1.7500, c = -1.5000, acc = 0.0;
        for (int i = 1; i <= 16; ++i) {
          acc = acc * 0.5 + (x * (double)i - c) / (double)(i + 1);
        }
        printf("%.6f\n", acc);
        return 0;
      }
