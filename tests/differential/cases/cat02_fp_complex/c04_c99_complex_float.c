#include <complex.h>
    #include <stdio.h>
    int main(void) {
      float _Complex a = 2.5f + 1.5f * I;
      float _Complex b = -0.5f + 2.0f * I;
      float _Complex prod = a * b;
      printf("%.2f %.2f\n", (double)crealf(prod), (double)cimagf(prod));
      return 0;
    }
